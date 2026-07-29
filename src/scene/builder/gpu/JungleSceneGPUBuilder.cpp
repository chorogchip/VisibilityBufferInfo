#include "scene/builder/gpu/JungleSceneGPUBuilder.h"

#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

#include "dx_util/ResourceUtils.h"
#include "scene/builder/gpu/BenchmarkSceneGPUBuilder.h"
#include "scene/builder/gpu/DonutSceneGPUBuilder.h"
#include "util/Logger.h"

namespace scene {

    namespace {

        void upload_buffer(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            const void* source,
            uint64_t byte_size,
            eng::GPUResource& destination,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps) {

            util::Logger::g_logger.assert_with_log(
                device != nullptr &&
                command_list != nullptr &&
                source != nullptr &&
                byte_size > 0,
                "Jungle compact GPU upload requires non-empty inputs.");
            auto upload = dxutl::create_buffer(
                device,
                byte_size,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);
            auto resource = dxutl::create_buffer(
                device,
                byte_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST);
            dxutl::copy_to_upload_buffer(
                upload.Get(),
                source,
                static_cast<size_t>(byte_size));
            command_list->CopyBufferRegion(
                resource.Get(),
                0,
                upload.Get(),
                0,
                byte_size);
            destination.init(
                resource.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST);
            destination.transition(
                command_list,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
            used_upload_heaps.emplace_back(std::move(upload));
        }

        uint32_t to_uint32(
            size_t value,
            const char* message) {

            util::Logger::g_logger.assert_with_log(
                value <= (std::numeric_limits<uint32_t>::max)(),
                message);
            return static_cast<uint32_t>(value);
        }

    } // namespace

    BenchmarkSceneGPUData JungleSceneGPUBuilder::build_benchmark(
        const SceneCPUData& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
            used_upload_heaps,
        bool load_textures) {

        util::Logger::g_logger <<
            "Jungle GPU entry: benchmark generic layout upload; vertices=" <<
            source.vertices.size() <<
            ", indices=" << source.indices.size() <<
            ", instances=" << source.instances.size() <<
            ", draw_instances=" << source.draw_instances.size() << ".\n";
        BenchmarkSceneGPUData result =
            BenchmarkSceneGPUBuilder::build(
            source,
            device,
            command_list,
            used_upload_heaps,
            load_textures);
        util::Logger::g_logger <<
            "Jungle GPU upload complete: benchmark vertices=" <<
            result.vertex_count <<
            ", indices=" << result.index_count <<
            ", instances=" << result.instance_count <<
            ", draw_instances=" << result.draw_instance_count << ".\n";
        return result;
    }

    DonutSceneGPUData JungleSceneGPUBuilder::build_donut(
        const SceneCPUData& source,
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
            used_upload_heaps,
        const std::function<void()>& flush_uploads,
        bool load_textures) {

        util::Logger::g_logger <<
            "Jungle GPU entry: Donut generic layout upload; vertices=" <<
            source.vertices.size() <<
            ", indices=" << source.indices.size() <<
            ", instances=" << source.instances.size() <<
            ", draw_instances=" << source.draw_instances.size() << ".\n";
        DonutSceneGPUData result = DonutSceneGPUBuilder::build(
            source,
            device,
            command_list,
            used_upload_heaps,
            flush_uploads,
            load_textures);
        util::Logger::g_logger <<
            "Jungle GPU upload complete: Donut vertex_bytes=" <<
            result.vertex_layout.byte_size <<
            ", indices=" << result.index_count <<
            ", instances=" << result.instance_data.size() <<
            ", draw_instances=" << result.draw_instance_data.size() <<
            ", geometry_instances=" <<
            result.geometry_instance_data.size() << ".\n";
        return result;
    }

    JungleDonutSceneGPUData
        JungleSceneGPUBuilder::build_donut_compact(
            const JungleSceneCPUData& source,
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
                used_upload_heaps,
            const std::function<void()>& flush_uploads,
            bool load_textures) {

        util::Logger::g_logger.assert_with_log(
            !source.point_instances.empty() &&
            source.point_instances.size() ==
                source.point_instance_ids_by_prototype.size() &&
            !source.point_prototypes.empty(),
            "Jungle compact Donut upload requires a complete "
            "PointInstancer stream.");

        JungleDonutSceneGPUData result{};
        result.scene = DonutSceneGPUBuilder::build(
            source.scene,
            device,
            command_list,
            used_upload_heaps,
            flush_uploads,
            load_textures);

        std::vector<JungleSceneGPUData::PointPrototypeData>
            prototypes;
        prototypes.reserve(source.point_prototypes.size());
        for (const auto& source_prototype :
            source.point_prototypes) {
            prototypes.push_back({
                source_prototype.prototype_local_transform,
                source_prototype.instancer_world_transform
            });
        }

        upload_buffer(
            device,
            command_list,
            source.point_instances.data(),
            static_cast<uint64_t>(
                source.point_instances.size()) *
                sizeof(JungleSceneCPUData::PointInstance),
            result.point_scene.point_instance_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            prototypes.data(),
            static_cast<uint64_t>(prototypes.size()) *
                sizeof(
                    JungleSceneGPUData::PointPrototypeData),
            result.point_scene.point_prototype_buffer,
            used_upload_heaps);
        upload_buffer(
            device,
            command_list,
            source.point_instance_ids_by_prototype.data(),
            static_cast<uint64_t>(
                source.point_instance_ids_by_prototype.size()) *
                sizeof(uint32_t),
            result.point_scene.point_instance_id_buffer,
            used_upload_heaps);

        result.point_scene.point_instance_count =
            to_uint32(
                source.point_instances.size(),
                "Jungle compact point instance count exceeds 32-bit "
                "GPU indexing.");
        result.point_scene.point_prototype_count =
            to_uint32(
                prototypes.size(),
                "Jungle compact point prototype count exceeds 32-bit "
                "GPU indexing.");
        result.point_scene.point_instance_id_capacity =
            to_uint32(
                source.point_instance_ids_by_prototype.size(),
                "Jungle compact point ID capacity exceeds 32-bit "
                "GPU indexing.");

        const uint64_t compact_gpu_bytes =
            static_cast<uint64_t>(
                source.point_instances.size()) *
                sizeof(JungleSceneCPUData::PointInstance) +
            static_cast<uint64_t>(prototypes.size()) *
                sizeof(
                    JungleSceneGPUData::PointPrototypeData) +
            static_cast<uint64_t>(
                source.point_instance_ids_by_prototype.size()) *
                sizeof(uint32_t);
        util::Logger::g_logger <<
            "Jungle compact GPU upload complete: ordinary_instances=" <<
            result.scene.instance_data.size() <<
            ", point_instances=" <<
            result.point_scene.point_instance_count <<
            ", point_prototypes=" <<
            result.point_scene.point_prototype_count <<
            ", compact_point_bytes=" <<
            compact_gpu_bytes <<
            ", generic_draw_instances=" <<
            result.scene.draw_instance_data.size() << ".\n";
        return result;
    }

} // namespace scene
