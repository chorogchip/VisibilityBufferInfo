#include "scene/builder/source/SceneRawJungleToSource.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>

#include <DirectXMath.h>

#include "scene/builder/source/SceneSourceSemanticValidator.h"
#include "scene/raw/SceneRawJungleUsdAccess.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/listOp.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/boundable.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/usd/usdGeom/pointInstancer.h>
#include <pxr/usd/usdGeom/primvarsAPI.h>
#include <pxr/usd/usdGeom/subset.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/materialBindingAPI.h>
#include <pxr/usd/usdShade/connectableAPI.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/usd/usdShade/tokens.h>

namespace scene {

    namespace pxr = ::pxr;

    namespace {

        constexpr uint32_t INVALID_INDEX = source::SceneConstants::INVALID_INDEX;

        uint64_t fnv1a_(const std::string& value) {
            uint64_t result = 14695981039346656037ull;
            for (const unsigned char byte : value) {
                result ^= byte;
                result *= 1099511628211ull;
            }
            return result;
        }

        std::string hex_(uint64_t value) {
            static constexpr char digits[] = "0123456789abcdef";
            std::string text(16, '0');
            for (int index = 15; index >= 0; --index) {
                text[static_cast<size_t>(index)] = digits[value & 0xfu];
                value >>= 4u;
            }
            return text;
        }

        std::string stable_id_(const std::string& value) {
            return "usd:" + hex_(fnv1a_(value));
        }

        source::SourceReference make_prim_reference_(
            const pxr::UsdPrim& prim,
            bool use_composition_path = false) {
            source::SourceReference result;
            result.source_type = prim.GetTypeName().GetString();
            result.prim_path = prim.GetPath().GetString();
            std::string identity = result.prim_path + "|" + result.source_type;
            const auto stack = prim.GetPrimStack();
            const auto spec_it = std::find_if(stack.begin(), stack.end(), [](const auto& spec) {
                return spec && spec->GetLayer();
            });
            if (spec_it != stack.end()) {
                const auto& spec = *spec_it;
                result.layer_identifier = spec->GetLayer()->GetIdentifier();
                if (use_composition_path) {
                    result.prim_path = spec->GetPath().GetString();
                }
                identity = result.layer_identifier + "|" +
                    spec->GetPath().GetString() + "|" + result.source_type;
            }
            result.stable_id = stable_id_(identity);
            return result;
        }

        source::SourceReference make_property_reference_(
            const pxr::UsdProperty& property) {
            source::SourceReference result = make_prim_reference_(property.GetPrim());
            result.property_path = property.GetPath().GetString();
            if (const auto attribute = property.As<pxr::UsdAttribute>()) {
                result.source_type = attribute.GetTypeName().GetAsToken().GetString();
            } else {
                result.source_type = "relationship";
            }
            result.stable_id = stable_id_(
                result.layer_identifier + "|" + result.property_path + "|" +
                result.source_type);
            return result;
        }

        DirectX::XMFLOAT4X4 matrix_(const pxr::GfMatrix4d& value) {
            return {
                static_cast<float>(value[0][0]), static_cast<float>(value[0][1]),
                static_cast<float>(value[0][2]), static_cast<float>(value[0][3]),
                static_cast<float>(value[1][0]), static_cast<float>(value[1][1]),
                static_cast<float>(value[1][2]), static_cast<float>(value[1][3]),
                static_cast<float>(value[2][0]), static_cast<float>(value[2][1]),
                static_cast<float>(value[2][2]), static_cast<float>(value[2][3]),
                static_cast<float>(value[3][0]), static_cast<float>(value[3][1]),
                static_cast<float>(value[3][2]), static_cast<float>(value[3][3]),
            };
        }

        math::AABB bounds_(const std::vector<DirectX::XMFLOAT3>& points) {
            math::AABB result = math::AABB::create_empty();
            for (const auto& point : points) {
                result = result.get_union(math::AABB::create_from_pos(point));
            }
            return result;
        }

        bool is_finite_(const math::AABB& bounds) {
            return !bounds.is_valid ||
                (std::isfinite(bounds.pos_min.x) && std::isfinite(bounds.pos_min.y) &&
                 std::isfinite(bounds.pos_min.z) && std::isfinite(bounds.pos_max.x) &&
                 std::isfinite(bounds.pos_max.y) && std::isfinite(bounds.pos_max.z));
        }

        source::PrimvarInterpolation interpolation_(const pxr::TfToken& value) {
            if (value == pxr::UsdGeomTokens->constant) return source::PrimvarInterpolation::Constant;
            if (value == pxr::UsdGeomTokens->uniform) return source::PrimvarInterpolation::Uniform;
            if (value == pxr::UsdGeomTokens->vertex) return source::PrimvarInterpolation::Vertex;
            if (value == pxr::UsdGeomTokens->varying) return source::PrimvarInterpolation::Varying;
            if (value == pxr::UsdGeomTokens->faceVarying) return source::PrimvarInterpolation::FaceVarying;
            if (value == pxr::TfToken("instance")) return source::PrimvarInterpolation::Instance;
            return source::PrimvarInterpolation::Unknown;
        }

        void append_diagnostic_(
            SceneSourceData& scene,
            source::ConversionSeverity severity,
            const source::SourceReference& reference,
            std::string code,
            std::string message) {
            scene.conversion_diagnostics.push_back({
                severity,
                reference,
                std::move(code),
                std::move(message),
            });
        }

        bool include_prim_(
            const pxr::UsdPrim& prim,
            const SceneRawJungleToSourceOptions& options,
            const pxr::UsdTimeCode& time) {
            if (!options.include_inactive_prims && !prim.IsActive()) {
                return false;
            }
            const pxr::UsdGeomImageable imageable(prim);
            if (!imageable) {
                return true;
            }
            const auto purpose = imageable.ComputePurpose();
            if (!options.include_invisible_prims &&
                imageable.ComputeVisibility(time) == pxr::UsdGeomTokens->invisible) {
                return false;
            }
            if (!options.include_proxy_purpose && purpose == pxr::UsdGeomTokens->proxy) {
                return false;
            }
            if (!options.include_guide_purpose && purpose == pxr::UsdGeomTokens->guide) {
                return false;
            }
            return true;
        }

        source::ShaderValue shader_value_(
            const pxr::UsdAttribute& attribute,
            const pxr::UsdTimeCode& time) {
            source::ShaderValue value;
            value.source = make_property_reference_(attribute);
            value.name = attribute.GetBaseName().GetString();
            value.value_type = attribute.GetTypeName().GetAsToken().GetString();
            value.authored = attribute.IsAuthored();
            pxr::VtValue composed_value;
            if (attribute.Get(&composed_value, time)) {
                value.authored_value = pxr::TfStringify(composed_value);
            }
            if (attribute.GetTypeName().GetScalarType() == pxr::SdfValueTypeNames->Asset) {
                pxr::SdfAssetPath asset;
                if (attribute.Get(&asset, time)) {
                    value.authored_asset_path = asset.GetAssetPath();
                    value.resolved_asset_path = asset.GetResolvedPath();
                }
            }
            return value;
        }

        void copy_numeric_primvar_(
            const pxr::UsdAttribute& attribute,
            const pxr::UsdTimeCode& time,
            source::Primvar& result) {
            const auto type = attribute.GetTypeName();
            if (type == pxr::SdfValueTypeNames->FloatArray) {
                pxr::VtFloatArray values;
                if (attribute.Get(&values, time)) {
                    result.numeric_component_count = 1;
                    result.float_values.assign(values.begin(), values.end());
                }
            } else if (type == pxr::SdfValueTypeNames->DoubleArray) {
                pxr::VtDoubleArray values;
                if (attribute.Get(&values, time)) {
                    result.numeric_component_count = 1;
                    result.double_values.assign(values.begin(), values.end());
                }
            } else if (type == pxr::SdfValueTypeNames->IntArray) {
                pxr::VtIntArray values;
                if (attribute.Get(&values, time)) {
                    result.numeric_component_count = 1;
                    result.integer_values.assign(values.begin(), values.end());
                }
            } else if (type == pxr::SdfValueTypeNames->Int64Array) {
                pxr::VtInt64Array values;
                if (attribute.Get(&values, time)) {
                    result.numeric_component_count = 1;
                    result.integer_values.assign(values.begin(), values.end());
                }
            } else if (type == pxr::SdfValueTypeNames->Float2Array) {
                pxr::VtVec2fArray values;
                if (attribute.Get(&values, time)) {
                    result.numeric_component_count = 2;
                    for (const auto& value : values) {
                        result.float_values.insert(result.float_values.end(), { value[0], value[1] });
                    }
                }
            } else if (type == pxr::SdfValueTypeNames->Float3Array ||
                type == pxr::SdfValueTypeNames->Vector3fArray ||
                type == pxr::SdfValueTypeNames->Normal3fArray ||
                type == pxr::SdfValueTypeNames->Point3fArray ||
                type == pxr::SdfValueTypeNames->Color3fArray) {
                pxr::VtVec3fArray values;
                if (attribute.Get(&values, time)) {
                    result.numeric_component_count = 3;
                    for (const auto& value : values) {
                        result.float_values.insert(result.float_values.end(), { value[0], value[1], value[2] });
                    }
                }
            } else if (type == pxr::SdfValueTypeNames->Float4Array ||
                type == pxr::SdfValueTypeNames->Color4fArray) {
                pxr::VtVec4fArray values;
                if (attribute.Get(&values, time)) {
                    result.numeric_component_count = 4;
                    for (const auto& value : values) {
                        result.float_values.insert(result.float_values.end(), { value[0], value[1], value[2], value[3] });
                    }
                }
            }
        }

        source::Primvar convert_primvar_(
            const pxr::UsdGeomPrimvar& primvar,
            const pxr::UsdTimeCode& time) {
            source::Primvar result;
            const auto attribute = primvar.GetAttr();
            result.source = make_property_reference_(attribute);
            result.name = primvar.GetPrimvarName().GetString();
            result.value_type = attribute.GetTypeName().GetAsToken().GetString();
            result.interpolation = interpolation_(primvar.GetInterpolation());
            result.element_size = primvar.GetElementSize();
            result.indexed = primvar.IsIndexed();
            result.time_varying = attribute.ValueMightBeTimeVarying();
            pxr::VtValue value;
            if (attribute.Get(&value, time)) {
                result.serialized_value = pxr::TfStringify(value);
            }
            copy_numeric_primvar_(attribute, time, result);
            if (result.indexed) {
                pxr::VtIntArray indices;
                if (primvar.GetIndicesAttr().Get(&indices, time)) {
                    result.indices.reserve(indices.size());
                    for (const auto index : indices) {
                        result.indices.push_back(static_cast<uint32_t>(index));
                    }
                }
            }
            return result;
        }

        void append_asset_(
            SceneSourceData& scene,
            const pxr::UsdAttribute& attribute,
            const pxr::UsdTimeCode& time) {
            if (attribute.GetTypeName().GetScalarType() != pxr::SdfValueTypeNames->Asset) {
                return;
            }
            pxr::SdfAssetPath asset;
            if (!attribute.Get(&asset, time) || asset.GetAssetPath().empty()) {
                return;
            }
            source::SourceAsset result;
            result.source = make_property_reference_(attribute);
            result.authored_path = asset.GetAssetPath();
            result.resolved_path = asset.GetResolvedPath();
            result.category = "asset";
            result.resolved = !result.resolved_path.empty();
            scene.source_assets.push_back(std::move(result));
        }

        void append_connections_(
            const pxr::UsdAttribute& destination,
            source::MaterialGraph& graph) {
            pxr::SdfPathVector connections;
            if (!destination || !destination.GetConnections(&connections)) {
                return;
            }
            for (const auto& connection : connections) {
                source::ShaderConnection converted;
                converted.source = make_property_reference_(destination);
                converted.source_property_path = connection.GetString();
                converted.destination_property_path = destination.GetPath().GetString();
                graph.connections.push_back(std::move(converted));
            }
        }

        std::string direct_material_source_id_(const pxr::UsdPrim& prim) {
            const pxr::UsdShadeMaterialBindingAPI binding_api(prim);
            const auto relationship = binding_api.GetDirectBindingRel();
            if (!relationship) {
                return {};
            }
            pxr::SdfPathVector targets;
            relationship.GetTargets(&targets);
            if (targets.empty()) {
                return {};
            }
            return make_prim_reference_(
                prim.GetStage()->GetPrimAtPath(targets.back())).stable_id;
        }

        uint32_t append_mesh_(
            const pxr::UsdGeomMesh& usd_mesh,
            const pxr::UsdTimeCode& time,
            SceneSourceData& scene) {
            source::PolygonMesh mesh;
            mesh.source = make_prim_reference_(usd_mesh.GetPrim());
            mesh.evaluated_time_code = time.GetValue();
            pxr::VtVec3fArray points;
            pxr::VtIntArray counts;
            pxr::VtIntArray indices;
            usd_mesh.GetPointsAttr().Get(&points, time);
            usd_mesh.GetFaceVertexCountsAttr().Get(&counts, time);
            usd_mesh.GetFaceVertexIndicesAttr().Get(&indices, time);
            mesh.points.reserve(points.size());
            for (const auto& point : points) {
                mesh.points.push_back({ point[0], point[1], point[2] });
            }
            mesh.face_vertex_counts.reserve(counts.size());
            for (const auto count : counts) {
                mesh.face_vertex_counts.push_back(static_cast<uint32_t>(count));
            }
            mesh.face_vertex_indices.reserve(indices.size());
            for (const auto index : indices) {
                mesh.face_vertex_indices.push_back(static_cast<uint32_t>(index));
            }
            pxr::TfToken subdivision;
            pxr::TfToken orientation;
            bool double_sided = false;
            usd_mesh.GetSubdivisionSchemeAttr().Get(&subdivision, time);
            usd_mesh.GetOrientationAttr().Get(&orientation, time);
            usd_mesh.GetDoubleSidedAttr().Get(&double_sided, time);
            mesh.subdivision_scheme = subdivision.GetString();
            mesh.orientation = orientation.GetString();
            mesh.double_sided = double_sided;
            mesh.bound_material_source_id = direct_material_source_id_(usd_mesh.GetPrim());
            mesh.local_bounds = bounds_(mesh.points);
            mesh.time_varying = usd_mesh.GetPointsAttr().ValueMightBeTimeVarying() ||
                usd_mesh.GetFaceVertexCountsAttr().ValueMightBeTimeVarying() ||
                usd_mesh.GetFaceVertexIndicesAttr().ValueMightBeTimeVarying();

            for (const auto& primvar : pxr::UsdGeomPrimvarsAPI(usd_mesh).GetPrimvars()) {
                mesh.primvars.push_back(convert_primvar_(primvar, time));
            }
            for (const auto& subset : pxr::UsdGeomSubset::GetAllGeomSubsets(usd_mesh)) {
                source::MaterialSubset converted;
                converted.source = make_prim_reference_(subset.GetPrim());
                pxr::VtIntArray face_indices;
                if (subset.GetIndicesAttr().Get(&face_indices, time)) {
                    for (const auto index : face_indices) {
                        converted.face_indices.push_back(static_cast<uint32_t>(index));
                    }
                }
                converted.material_source_id = direct_material_source_id_(subset.GetPrim());
                mesh.material_subsets.push_back(std::move(converted));
            }
            const uint32_t id = static_cast<uint32_t>(scene.polygon_meshes.size());
            scene.polygon_meshes.push_back(std::move(mesh));
            return id;
        }

        void append_point_instancer_(
            const pxr::UsdGeomPointInstancer& usd_instancer,
            const pxr::UsdTimeCode& time,
            uint32_t node_id,
            SceneSourceData& scene) {
            source::PointInstancer result;
            result.source = make_prim_reference_(usd_instancer.GetPrim());
            result.node_id = node_id;
            pxr::SdfPathVector prototypes;
            usd_instancer.GetPrototypesRel().GetTargets(&prototypes);
            for (const auto& prototype_path : prototypes) {
                const auto prototype = usd_instancer.GetPrim().GetStage()->GetPrimAtPath(prototype_path);
                result.prototype_source_ids.push_back(make_prim_reference_(prototype, true).stable_id);
            }
            pxr::VtIntArray proto_indices;
            pxr::VtVec3fArray positions;
            pxr::VtQuathArray orientations;
            pxr::VtVec3fArray scales;
            pxr::VtVec3fArray velocities;
            pxr::VtVec3fArray angular_velocities;
            pxr::VtInt64Array ids;
            usd_instancer.GetProtoIndicesAttr().Get(&proto_indices, time);
            usd_instancer.GetPositionsAttr().Get(&positions, time);
            usd_instancer.GetOrientationsAttr().Get(&orientations, time);
            usd_instancer.GetScalesAttr().Get(&scales, time);
            usd_instancer.GetVelocitiesAttr().Get(&velocities, time);
            usd_instancer.GetAngularVelocitiesAttr().Get(&angular_velocities, time);
            usd_instancer.GetIdsAttr().Get(&ids, time);
            for (const auto value : proto_indices) result.proto_indices.push_back(value);
            for (const auto& value : positions) result.positions.push_back({ value[0], value[1], value[2] });
            for (const auto& value : orientations) {
                result.orientations.push_back({ value.GetImaginary()[0], value.GetImaginary()[1], value.GetImaginary()[2], value.GetReal() });
            }
            for (const auto& value : scales) result.scales.push_back({ value[0], value[1], value[2] });
            for (const auto& value : velocities) result.velocities.push_back({ value[0], value[1], value[2] });
            for (const auto& value : angular_velocities) result.angular_velocities.push_back({ value[0], value[1], value[2] });
            result.ids.assign(ids.begin(), ids.end());
            pxr::VtInt64Array invisible_ids;
            usd_instancer.GetInvisibleIdsAttr().Get(&invisible_ids, time);
            result.invisible_ids.assign(invisible_ids.begin(), invisible_ids.end());
            pxr::SdfInt64ListOp inactive_ids;
            if (usd_instancer.GetPrim().GetMetadata(
                pxr::UsdGeomTokens->inactiveIds,
                &inactive_ids)) {
                const auto& explicit_ids = inactive_ids.GetExplicitItems();
                result.inactive_ids.assign(explicit_ids.begin(), explicit_ids.end());
            }
            result.logical_instance_count = result.proto_indices.size();
            result.prototype_instance_counts.resize(result.prototype_source_ids.size(), 0);
            for (const auto index : result.proto_indices) {
                if (index >= 0 && static_cast<size_t>(index) < result.prototype_instance_counts.size()) {
                    ++result.prototype_instance_counts[static_cast<size_t>(index)];
                }
            }
            result.local_bounds = bounds_(result.positions);
            result.time_varying = usd_instancer.GetProtoIndicesAttr().ValueMightBeTimeVarying() ||
                usd_instancer.GetPositionsAttr().ValueMightBeTimeVarying() ||
                usd_instancer.GetOrientationsAttr().ValueMightBeTimeVarying() ||
                usd_instancer.GetScalesAttr().ValueMightBeTimeVarying();
            scene.point_instancers.push_back(std::move(result));
        }

        uint32_t append_shader_(
            const pxr::UsdShadeShader& shader,
            const pxr::UsdTimeCode& time,
            SceneSourceData& scene) {
            source::ShaderNode result;
            result.source = make_prim_reference_(shader.GetPrim());
            pxr::TfToken shader_id;
            shader.GetShaderId(&shader_id);
            result.shader_id = shader_id.GetString();
            for (const auto& input : shader.GetInputs()) {
                result.inputs.push_back(shader_value_(input.GetAttr(), time));
            }
            for (const auto& output : shader.GetOutputs()) {
                result.outputs.push_back(shader_value_(output.GetAttr(), time));
            }
            const uint32_t id = static_cast<uint32_t>(scene.shader_nodes.size());
            scene.shader_nodes.push_back(std::move(result));
            return id;
        }

        void append_material_bindings_(
            const pxr::UsdPrim& prim,
            SceneSourceData& scene) {
            if (!prim.HasAPI<pxr::UsdShadeMaterialBindingAPI>()) {
                return;
            }
            const pxr::UsdShadeMaterialBindingAPI api(prim);
            for (const auto& purpose : pxr::UsdShadeMaterialBindingAPI::GetMaterialPurposes()) {
                const auto append_binding = [&](const pxr::UsdRelationship& relationship, const char* kind) {
                    if (!relationship || !relationship.HasAuthoredTargets()) return;
                    source::MaterialBinding binding;
                    binding.source = make_property_reference_(relationship);
                    binding.purpose = purpose.GetString();
                    binding.binding_kind = kind;
                    binding.strength = pxr::UsdShadeMaterialBindingAPI::GetMaterialBindingStrength(relationship).GetString();
                    pxr::SdfPathVector targets;
                    relationship.GetTargets(&targets);
                    for (const auto& target : targets) {
                        binding.collection_targets.push_back(target.GetString());
                    }
                    if (!targets.empty()) {
                        binding.material_source_id = make_prim_reference_(prim.GetStage()->GetPrimAtPath(targets.back())).stable_id;
                    }
                    scene.material_bindings.push_back(std::move(binding));
                };
                append_binding(api.GetDirectBindingRel(purpose), "direct");
                for (const auto& relationship : api.GetCollectionBindingRels(purpose)) {
                    append_binding(relationship, "collection");
                }
            }
        }

        math::AABB compute_world_hierarchy_(
            SceneSourceData& scene,
            uint32_t node_id,
            DirectX::FXMMATRIX parent_world) {
            auto& node = scene.nodes[node_id];
            const auto local = DirectX::XMLoadFloat4x4(&node.local_transform);
            const auto world = node.reset_xform_stack
                ? local
                : DirectX::XMMatrixMultiply(local, parent_world);
            DirectX::XMStoreFloat4x4(&node.world_transform, world);

            math::AABB subtree = math::AABB::create_empty();
            if (node.polygon_mesh_id != INVALID_INDEX) {
                node.world_bounds = scene.polygon_meshes[node.polygon_mesh_id]
                    .local_bounds.get_transformed(node.world_transform);
                subtree = subtree.get_union(node.world_bounds);
            }
            for (const auto child_id : node.children) {
                subtree = subtree.get_union(
                    compute_world_hierarchy_(scene, child_id, world));
            }
            if (!node.world_bounds.is_valid) {
                node.world_bounds = subtree;
            }
            return subtree;
        }

    } // namespace

    std::unique_ptr<SceneSourceData> SceneRawJungleToSource::build(
        const raw::SceneRawJungle& raw_scene,
        const SceneRawJungleToSourceOptions& options) {
        const auto& stage = raw::SceneRawJungleUsdAccess::stage(raw_scene);
        const double evaluated_time = std::isfinite(options.time_code)
            ? options.time_code
            : stage->GetStartTimeCode();
        const pxr::UsdTimeCode time(evaluated_time);

        auto scene = std::make_unique<SceneSourceData>();
        const auto& raw_stage = raw_scene.stage_info();
        scene->metadata.source_scene_identifier = raw_stage.root_layer_identifier;
        scene->metadata.root_usd_path = raw_scene.root_path().generic_string();
        scene->metadata.default_prim_path = raw_stage.default_prim_path;
        scene->metadata.up_axis = raw_stage.up_axis;
        scene->metadata.coordinate_system = "USD " + raw_stage.up_axis + "-up";
        scene->metadata.meters_per_unit = raw_stage.meters_per_unit;
        scene->metadata.start_time_code = raw_stage.start_time_code;
        scene->metadata.end_time_code = raw_stage.end_time_code;
        scene->metadata.frames_per_second = raw_stage.frames_per_second;
        scene->metadata.time_codes_per_second = raw_stage.time_codes_per_second;
        scene->metadata.evaluated_time_code = evaluated_time;

        source::Node root;
        root.name = "USD PseudoRoot";
        root.kind = source::NodeKind::SceneRoot;
        root.source.stable_id = stable_id_(raw_stage.root_layer_identifier + "|/");
        root.source.prim_path = "/";
        root.source.layer_identifier = raw_stage.root_layer_identifier;
        root.source.source_type = "PseudoRoot";
        root.stable_id = root.source.stable_id;
        root.prim_type = root.source.source_type;
        scene->nodes.push_back(std::move(root));
        scene->root_node_id = 0;

        std::unordered_map<std::string, uint32_t> nodes_by_path;
        nodes_by_path.emplace("/", scene->root_node_id);
        std::unordered_map<std::string, uint32_t> material_graph_by_path;

        const auto append_node = [&](const pxr::UsdPrim& prim, bool prototype) {
            source::Node node;
            node.name = prim.GetName().GetString();
            node.source = make_prim_reference_(prim, prototype);
            node.stable_id = node.source.stable_id;
            node.prim_type = prim.GetTypeName().GetString();
            node.active = prim.IsActive();
            node.loaded = prim.IsLoaded();
            node.defined = prim.IsDefined();
            node.abstract = prim.IsAbstract();
            node.native_instance = prim.IsInstance();
            for (const auto& schema : prim.GetAppliedSchemas()) {
                node.applied_schemas.push_back(schema.GetString());
            }
            const pxr::UsdGeomImageable imageable(prim);
            if (imageable) {
                node.purpose = imageable.ComputePurpose().GetString();
                node.visible = imageable.ComputeVisibility(time) != pxr::UsdGeomTokens->invisible;
            }
            pxr::GfMatrix4d local(1.0);
            bool resets = false;
            const pxr::UsdGeomXformable xformable(prim);
            if (xformable) {
                xformable.GetLocalTransformation(&local, &resets, time);
            }
            node.local_transform = matrix_(local);
            node.world_transform = node.local_transform;
            node.reset_xform_stack = resets;

            const auto parent_path = prim.GetParent().GetPath().GetString();
            const auto parent_it = nodes_by_path.find(parent_path);
            node.parent_node_id = parent_it == nodes_by_path.end()
                ? scene->root_node_id
                : parent_it->second;
            const uint32_t id = static_cast<uint32_t>(scene->nodes.size());
            scene->nodes[node.parent_node_id].children.push_back(id);
            scene->nodes.push_back(std::move(node));
            nodes_by_path.emplace(prim.GetPath().GetString(), id);
            return id;
        };

        for (const auto& prim : stage->TraverseAll()) {
            if (!include_prim_(prim, options, time)) {
                continue;
            }
            const uint32_t node_id = append_node(prim, false);
            for (const auto& attribute : prim.GetAttributes()) {
                append_asset_(*scene, attribute, time);
            }
            append_material_bindings_(prim, *scene);
            if (prim.IsA<pxr::UsdGeomMesh>()) {
                scene->nodes[node_id].polygon_mesh_id = append_mesh_(pxr::UsdGeomMesh(prim), time, *scene);
            }
            if (options.preserve_point_instancers && prim.IsA<pxr::UsdGeomPointInstancer>()) {
                append_point_instancer_(pxr::UsdGeomPointInstancer(prim), time, node_id, *scene);
            }
            if (prim.IsA<pxr::UsdGeomCamera>()) {
                const pxr::UsdGeomCamera camera(prim);
                source::SourceCamera converted;
                converted.source = make_prim_reference_(prim);
                converted.node_id = node_id;
                pxr::TfToken projection;
                camera.GetProjectionAttr().Get(&projection, time);
                converted.projection = projection.GetString();
                camera.GetFocalLengthAttr().Get(&converted.focal_length, time);
                camera.GetHorizontalApertureAttr().Get(&converted.horizontal_aperture, time);
                camera.GetVerticalApertureAttr().Get(&converted.vertical_aperture, time);
                camera.GetHorizontalApertureOffsetAttr().Get(&converted.horizontal_aperture_offset, time);
                camera.GetVerticalApertureOffsetAttr().Get(&converted.vertical_aperture_offset, time);
                camera.GetFocusDistanceAttr().Get(&converted.focus_distance, time);
                camera.GetFStopAttr().Get(&converted.f_stop, time);
                camera.GetExposureAttr().Get(&converted.exposure, time);
                pxr::GfVec2f clipping;
                if (camera.GetClippingRangeAttr().Get(&clipping, time)) {
                    converted.clipping_range = { clipping[0], clipping[1] };
                }
                converted.time_varying = camera.GetFocalLengthAttr().ValueMightBeTimeVarying();
                scene->source_cameras.push_back(std::move(converted));
            }
            if (prim.HasAPI<pxr::UsdLuxLightAPI>()) {
                source::SourceLight light;
                light.source = make_prim_reference_(prim);
                light.node_id = node_id;
                light.light_type = prim.GetTypeName().GetString();
                const pxr::UsdLuxLightAPI light_api(prim);
                pxr::GfVec3f color(1.0f);
                light_api.GetColorAttr().Get(&color, time);
                light.color = { color[0], color[1], color[2] };
                light_api.GetIntensityAttr().Get(&light.intensity, time);
                light_api.GetExposureAttr().Get(&light.exposure, time);
                for (const auto& input : light_api.GetInputs()) {
                    light.parameters.push_back(shader_value_(input.GetAttr(), time));
                }
                scene->source_lights.push_back(std::move(light));
            }
            if (prim.IsA<pxr::UsdShadeMaterial>()) {
                source::MaterialGraph graph;
                graph.source = make_prim_reference_(prim);
                const uint32_t graph_id = static_cast<uint32_t>(scene->material_graphs.size());
                material_graph_by_path.emplace(prim.GetPath().GetString(), graph_id);
                scene->material_graphs.push_back(std::move(graph));
                const pxr::UsdShadeConnectableAPI connectable(prim);
                for (const auto& input : connectable.GetInputs()) {
                    append_connections_(input.GetAttr(), scene->material_graphs[graph_id]);
                }
                for (const auto& output : connectable.GetOutputs()) {
                    append_connections_(output.GetAttr(), scene->material_graphs[graph_id]);
                }
            } else if (prim.IsA<pxr::UsdShadeNodeGraph>()) {
                source::MaterialGraph graph;
                graph.source = make_prim_reference_(prim);
                const uint32_t graph_id = static_cast<uint32_t>(scene->material_graphs.size());
                material_graph_by_path.emplace(prim.GetPath().GetString(), graph_id);
                scene->material_graphs.push_back(std::move(graph));
                const pxr::UsdShadeConnectableAPI connectable(prim);
                for (const auto& input : connectable.GetInputs()) {
                    append_connections_(input.GetAttr(), scene->material_graphs[graph_id]);
                }
                for (const auto& output : connectable.GetOutputs()) {
                    append_connections_(output.GetAttr(), scene->material_graphs[graph_id]);
                }
            }
            if (prim.IsA<pxr::UsdShadeShader>()) {
                const uint32_t shader_id = append_shader_(pxr::UsdShadeShader(prim), time, *scene);
                auto ancestor = prim.GetParent();
                while (ancestor) {
                    const auto material_it = material_graph_by_path.find(ancestor.GetPath().GetString());
                    if (material_it != material_graph_by_path.end()) {
                        auto& graph = scene->material_graphs[material_it->second];
                        graph.shader_node_ids.push_back(shader_id);
                        const pxr::UsdShadeShader shader(prim);
                        for (const auto& input : shader.GetInputs()) {
                            append_connections_(input.GetAttr(), graph);
                        }
                        for (const auto& output : shader.GetOutputs()) {
                            append_connections_(output.GetAttr(), graph);
                        }
                        break;
                    }
                    ancestor = ancestor.GetParent();
                }
            }
        }

        if (options.preserve_native_instances) {
            std::set<std::string> prototype_ids;
            for (const auto& prototype : stage->GetPrototypes()) {
                source::NativePrototype converted;
                converted.source = make_prim_reference_(prototype, true);
                if (!prototype_ids.insert(converted.source.stable_id).second) continue;
                std::unordered_map<std::string, uint32_t> prototype_nodes;
                for (const auto& prototype_prim : pxr::UsdPrimRange::AllPrims(prototype)) {
                    const uint32_t node_id = append_node(prototype_prim, true);
                    prototype_nodes.emplace(prototype_prim.GetPath().GetString(), node_id);
                    converted.node_ids.push_back(node_id);
                    if (converted.node_ids.size() == 1) converted.root_node_id = node_id;
                    if (prototype_prim.IsA<pxr::UsdGeomMesh>()) {
                        scene->nodes[node_id].polygon_mesh_id = append_mesh_(pxr::UsdGeomMesh(prototype_prim), time, *scene);
                    }
                }
                scene->native_prototypes.push_back(std::move(converted));
            }
            for (uint32_t node_id = 0; node_id < scene->nodes.size(); ++node_id) {
                const auto& node = scene->nodes[node_id];
                if (!node.native_instance || node.source.prim_path.empty()) continue;
                const auto instance_prim = stage->GetPrimAtPath(
                    pxr::SdfPath(node.source.prim_path));
                if (!instance_prim) continue;
                source::NativeInstance instance;
                instance.source = node.source;
                instance.node_id = node_id;
                instance.prototype_source_id = make_prim_reference_(instance_prim.GetPrototype(), true).stable_id;
                instance.has_composed_overrides = instance_prim.GetPrimStack().size() > 1;
                scene->native_instances.push_back(std::move(instance));
            }
        }

        compute_world_hierarchy_(
            *scene,
            scene->root_node_id,
            DirectX::XMMatrixIdentity());

        const std::vector<pxr::TfToken> purposes = {
            pxr::UsdGeomTokens->default_,
            pxr::UsdGeomTokens->render,
            pxr::UsdGeomTokens->proxy,
            pxr::UsdGeomTokens->guide,
        };
        pxr::UsdGeomBBoxCache bounds_cache(time, purposes, true);
        const auto bounds = bounds_cache.ComputeWorldBound(stage->GetPseudoRoot())
            .ComputeAlignedRange();
        if (!bounds.IsEmpty()) {
            scene->metadata.source_bounds.is_valid = true;
            scene->metadata.source_bounds.pos_min = {
                static_cast<float>(bounds.GetMin()[0]),
                static_cast<float>(bounds.GetMin()[1]),
                static_cast<float>(bounds.GetMin()[2]),
            };
            scene->metadata.source_bounds.pos_max = {
                static_cast<float>(bounds.GetMax()[0]),
                static_cast<float>(bounds.GetMax()[1]),
                static_cast<float>(bounds.GetMax()[2]),
            };
        }
        if (!is_finite_(scene->metadata.source_bounds)) {
            append_diagnostic_(*scene, source::ConversionSeverity::Error, {}, "non_finite_bounds", "Computed source bounds are non-finite.");
        }
        SceneSourceSemanticValidator::validate(*scene);
        return scene;
    }

} // namespace scene
