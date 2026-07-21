#include "scene/Material.h"

#include <initializer_list>
#include <string>
#include <utility>

#include <assimp/material.h>
#include <assimp/pbrmaterial.h>

#include "util/Logger.h"

namespace scene {

    namespace {

        TextureCPU read_texture(
            const aiMaterial& material,
            std::initializer_list<aiTextureType> texture_types,
            const std::filesystem::path& scene_path) {

            for (const aiTextureType type : texture_types) {

                if (material.GetTextureCount(type) == 0) continue;

                aiString assimp_path;
                if (material.GetTexture(type, 0, &assimp_path) != AI_SUCCESS) {
                    continue;
                }

                const std::string raw_path = assimp_path.C_Str();

                if (raw_path.empty()) {
                    continue;
                }

                if (raw_path.front() == '*') {
                    util::Logger::g_logger <<
                        "[Material]: not yet supoort assimp embedded texture\n\ton [" <<
                        raw_path << "[\n";
                    util::Logger::g_logger.assert_with_log(false);
                }

                std::filesystem::path resolved_path = raw_path;

                if (resolved_path.is_relative()) {
                    resolved_path =
                        scene_path.parent_path() / resolved_path;
                }

                resolved_path =
                    resolved_path.lexically_normal();

                TextureCPU result;
                result.name =
                    resolved_path.filename().string();
                result.path =
                    std::move(resolved_path);

                return result;
            }

            return {};
        }

    }

    std::vector<MaterialCPU> MaterialCPU::from_assimp(
        void* materials,
        size_t material_cnt,
        const std::filesystem::path& scene_path) {

        std::vector<MaterialCPU> ret;
        ret.reserve(material_cnt);

        if (materials == nullptr || material_cnt == 0) {
            return ret;
        }

        auto** assimp_materials =
            static_cast<aiMaterial**>(materials);

        for (size_t i = 0; i < material_cnt; ++i) {
            const aiMaterial* source = assimp_materials[i];

            MaterialCPU material;
            material.name =
                "material_" + std::to_string(i);

            if (source == nullptr) {
                ret.emplace_back(std::move(material));
                continue;
            }

            // Material name
            aiString material_name;
            if (source->Get(
                AI_MATKEY_NAME,
                material_name) == AI_SUCCESS &&
                material_name.length > 0) {
                material.name = material_name.C_Str();
            }

            // Base color. PBR base color가 없으면 diffuse로 대체한다.
            aiColor4D base_color;

            if (aiGetMaterialColor(
                source,
                AI_MATKEY_BASE_COLOR,
                &base_color) == AI_SUCCESS ||
                aiGetMaterialColor(
                    source,
                    AI_MATKEY_COLOR_DIFFUSE,
                    &base_color) == AI_SUCCESS) {
                material.color_base = {
                    base_color.r,
                    base_color.g,
                    base_color.b
                };
            }

            // Emissive color
            aiColor3D emissive_color;
            if (source->Get(
                AI_MATKEY_COLOR_EMISSIVE,
                emissive_color) == AI_SUCCESS) {
                material.color_emissive = {
                    emissive_color.r,
                    emissive_color.g,
                    emissive_color.b
                };
            }

            // Base color / diffuse
            material.tex_base_color = read_texture(
                *source,
                {
                    aiTextureType_BASE_COLOR,
                    aiTextureType_DIFFUSE
                },
                scene_path);

            // 빠른 구현을 위해 metallic과 roughness 중
            // 먼저 발견되는 하나를 사용한다.
            material.tex_metal_roughness = read_texture(
                *source,
                {
                    aiTextureType_METALNESS,
                    aiTextureType_DIFFUSE_ROUGHNESS
                },
                scene_path);

            // Normal. 일부 OBJ/FBX importer는 normal map을
            // HEIGHT 타입으로 보고할 수 있다.
            material.tex_normal = read_texture(
                *source,
                {
                    aiTextureType_NORMALS,
                    aiTextureType_HEIGHT
                },
                scene_path);

            material.tex_emissive = read_texture(
                *source,
                {
                    aiTextureType_EMISSIVE
                },
                scene_path);

            // 일부 포맷에서는 AO가 LIGHTMAP으로 반환된다.
            material.tex_occlusion = read_texture(
                *source,
                {
                    aiTextureType_AMBIENT_OCCLUSION,
                    aiTextureType_LIGHTMAP
                },
                scene_path);

            ret.emplace_back(std::move(material));
        }

        return ret;
    }

}
