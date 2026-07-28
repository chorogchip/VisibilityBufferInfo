#include "scene/cache/SceneCPUCache.h"

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "scene/builder/cpu/SceneCPUBuilder.h"
#include "scene/builder/source/SceneSourceFactory.h"
#include "util/Logger.h"

namespace scene {

    namespace {
        constexpr std::array<char, 8> CACHE_MAGIC = {
            'T', 'V', 'B', 'S', 'C', 'P', 'U', '1'
        };
        constexpr std::uint32_t CACHE_VERSION = 1;
        constexpr std::uint64_t MAX_VECTOR_ELEMENT_COUNT = 1ull << 30;

        struct CacheHeader {
            std::array<char, 8> magic{};
            std::uint32_t version = 0;
            std::uint32_t reserved = 0;
            std::uint64_t source_key = 0;
            std::uint64_t source_size = 0;
            std::int64_t source_write_time = 0;
        };
        static_assert(std::is_trivially_copyable_v<CacheHeader>);

        struct CacheIdentity {
            std::filesystem::path cache_path;
            std::uint64_t source_key = 0;
            std::uint64_t source_size = 0;
            std::int64_t source_write_time = 0;
        };

        std::uint64_t fnv1a(std::string_view text) {
            std::uint64_t hash = 14695981039346656037ull;
            for (const char value : text) {
                hash ^= static_cast<std::uint8_t>(value);
                hash *= 1099511628211ull;
            }
            return hash;
        }

        std::string path_to_utf8(const std::filesystem::path& path) {
            const std::u8string value = path.u8string();
            return {
                reinterpret_cast<const char*>(value.data()),
                value.size()
            };
        }

        std::filesystem::path path_from_utf8(const std::string& value) {
            return std::filesystem::u8path(value);
        }

        bool make_identity(
            const util::ProgramArgument& argument,
            CacheIdentity& result) {

            if (!argument.to_use_scene)
                return false;

            std::error_code error;
            const std::filesystem::path source_path =
                std::filesystem::absolute(argument.scene_path, error)
                    .lexically_normal();
            if (error || !std::filesystem::is_regular_file(source_path, error))
                return false;

            const std::uintmax_t source_size =
                std::filesystem::file_size(source_path, error);
            if (error || source_size >
                (std::numeric_limits<std::uint64_t>::max)()) {
                return false;
            }

            const std::filesystem::file_time_type source_write_time =
                std::filesystem::last_write_time(source_path, error);
            if (error)
                return false;

            const std::string source_path_utf8 = path_to_utf8(source_path);
            const std::string key_text = source_path_utf8 + "|" +
                argument.scene_importer;
            result.source_key = fnv1a(key_text);
            result.source_size = static_cast<std::uint64_t>(source_size);
            result.source_write_time = static_cast<std::int64_t>(
                source_write_time.time_since_epoch().count());
            error.clear();
            result.cache_path = std::filesystem::current_path(error) /
                ("tvb_scene_cache_" +
                    std::to_string(result.source_key) + ".bin");
            return !error;
        }

        template <typename T>
        bool write_value(std::ostream& output, const T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            output.write(
                reinterpret_cast<const char*>(&value), sizeof(value));
            return static_cast<bool>(output);
        }

        template <typename T>
        bool read_value(std::istream& input, T& value) {
            static_assert(std::is_trivially_copyable_v<T>);
            input.read(reinterpret_cast<char*>(&value), sizeof(value));
            return static_cast<bool>(input);
        }

        template <typename T>
        bool write_vector(
            std::ostream& output,
            const std::vector<T>& values) {

            static_assert(std::is_trivially_copyable_v<T>);
            const std::uint64_t count = values.size();
            if (!write_value(output, count))
                return false;
            if (count == 0)
                return true;
            output.write(
                reinterpret_cast<const char*>(values.data()),
                static_cast<std::streamsize>(count * sizeof(T)));
            return static_cast<bool>(output);
        }

        template <typename T>
        bool read_vector(std::istream& input, std::vector<T>& values) {
            static_assert(std::is_trivially_copyable_v<T>);
            std::uint64_t count = 0;
            if (!read_value(input, count) ||
                count > MAX_VECTOR_ELEMENT_COUNT ||
                count > (std::numeric_limits<size_t>::max)() / sizeof(T)) {
                return false;
            }

            values.resize(static_cast<size_t>(count));
            if (count == 0)
                return true;
            input.read(
                reinterpret_cast<char*>(values.data()),
                static_cast<std::streamsize>(count * sizeof(T)));
            return static_cast<bool>(input);
        }

        bool write_string(std::ostream& output, const std::string& value) {
            const std::uint64_t count = value.size();
            if (!write_value(output, count))
                return false;
            output.write(value.data(), static_cast<std::streamsize>(count));
            return static_cast<bool>(output);
        }

        bool read_string(std::istream& input, std::string& value) {
            std::uint64_t count = 0;
            if (!read_value(input, count) ||
                count > MAX_VECTOR_ELEMENT_COUNT ||
                count > (std::numeric_limits<size_t>::max)()) {
                return false;
            }
            value.resize(static_cast<size_t>(count));
            input.read(value.data(), static_cast<std::streamsize>(count));
            return static_cast<bool>(input);
        }

        bool write_texture_path(
            std::ostream& output,
            const SceneCPUData::Material::TexturePath& value) {

            const std::uint8_t exists = value ? 1 : 0;
            return write_value(output, exists) &&
                (!value || write_string(output, path_to_utf8(*value)));
        }

        bool read_texture_path(
            std::istream& input,
            SceneCPUData::Material::TexturePath& value) {

            std::uint8_t exists = 0;
            if (!read_value(input, exists) || exists > 1)
                return false;
            if (exists == 0) {
                value.reset();
                return true;
            }

            std::string path;
            if (!read_string(input, path))
                return false;
            value = path_from_utf8(path);
            return true;
        }

        bool write_material(
            std::ostream& output,
            const SceneCPUData::Material& material) {

            const std::uint32_t alpha_mode =
                static_cast<std::uint32_t>(material.alpha_mode);
            const std::uint8_t double_sided = material.double_sided ? 1 : 0;
            return
                write_value(output, material.base_color) &&
                write_value(output, material.emissive_color) &&
                write_value(output, material.emissive_intensity) &&
                write_value(output, material.metalness) &&
                write_value(output, material.roughness) &&
                write_value(output, material.opacity) &&
                write_value(output, material.alpha_cutoff) &&
                write_value(output, material.normal_scale) &&
                write_value(output, material.occlusion_strength) &&
                write_value(output, alpha_mode) &&
                write_value(output, double_sided) &&
                write_value(output, material.virtual_shader_id) &&
                write_texture_path(output, material.base_color_texture) &&
                write_texture_path(
                    output, material.metal_roughness_texture) &&
                write_texture_path(output, material.normal_texture) &&
                write_texture_path(output, material.emissive_texture) &&
                write_texture_path(output, material.occlusion_texture);
        }

        bool read_material(
            std::istream& input,
            SceneCPUData::Material& material) {

            std::uint32_t alpha_mode = 0;
            std::uint8_t double_sided = 0;
            if (!read_value(input, material.base_color) ||
                !read_value(input, material.emissive_color) ||
                !read_value(input, material.emissive_intensity) ||
                !read_value(input, material.metalness) ||
                !read_value(input, material.roughness) ||
                !read_value(input, material.opacity) ||
                !read_value(input, material.alpha_cutoff) ||
                !read_value(input, material.normal_scale) ||
                !read_value(input, material.occlusion_strength) ||
                !read_value(input, alpha_mode) ||
                alpha_mode > static_cast<std::uint32_t>(
                    source::AlphaMode::Blend) ||
                !read_value(input, double_sided) ||
                double_sided > 1 ||
                !read_value(input, material.virtual_shader_id) ||
                !read_texture_path(input, material.base_color_texture) ||
                !read_texture_path(
                    input, material.metal_roughness_texture) ||
                !read_texture_path(input, material.normal_texture) ||
                !read_texture_path(input, material.emissive_texture) ||
                !read_texture_path(input, material.occlusion_texture)) {
                return false;
            }

            material.alpha_mode = static_cast<source::AlphaMode>(alpha_mode);
            material.double_sided = double_sided != 0;
            return true;
        }

        bool write_materials(
            std::ostream& output,
            const std::vector<SceneCPUData::Material>& materials) {

            const std::uint64_t count = materials.size();
            if (!write_value(output, count))
                return false;
            for (const SceneCPUData::Material& material : materials) {
                if (!write_material(output, material))
                    return false;
            }
            return true;
        }

        bool read_materials(
            std::istream& input,
            std::vector<SceneCPUData::Material>& materials) {

            std::uint64_t count = 0;
            if (!read_value(input, count) ||
                count > MAX_VECTOR_ELEMENT_COUNT ||
                count > (std::numeric_limits<size_t>::max)()) {
                return false;
            }
            materials.resize(static_cast<size_t>(count));
            for (SceneCPUData::Material& material : materials) {
                if (!read_material(input, material))
                    return false;
            }
            return true;
        }

        bool write_nodes(
            std::ostream& output,
            const std::vector<SceneCPUData::Node>& nodes) {

            const std::uint64_t count = nodes.size();
            if (!write_value(output, count))
                return false;
            for (const SceneCPUData::Node& node : nodes) {
                if (!write_vector(output, node.children) ||
                    !write_value(output, node.local_transform) ||
                    !write_value(output, node.world_transform) ||
                    !write_value(output, node.mesh_id) ||
                    !write_value(output, node.first_instance) ||
                    !write_value(output, node.instance_count) ||
                    !write_value(output, node.subtree_world_aabb)) {
                    return false;
                }
            }
            return true;
        }

        bool read_nodes(
            std::istream& input,
            std::vector<SceneCPUData::Node>& nodes) {

            std::uint64_t count = 0;
            if (!read_value(input, count) ||
                count > MAX_VECTOR_ELEMENT_COUNT ||
                count > (std::numeric_limits<size_t>::max)()) {
                return false;
            }
            nodes.resize(static_cast<size_t>(count));
            for (SceneCPUData::Node& node : nodes) {
                if (!read_vector(input, node.children) ||
                    !read_value(input, node.local_transform) ||
                    !read_value(input, node.world_transform) ||
                    !read_value(input, node.mesh_id) ||
                    !read_value(input, node.first_instance) ||
                    !read_value(input, node.instance_count) ||
                    !read_value(input, node.subtree_world_aabb)) {
                    return false;
                }
            }
            return true;
        }

        bool write_scene(
            const std::filesystem::path& path,
            const CacheIdentity& identity,
            const SceneCPUData& scene) {

            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            if (!output)
                return false;

            const CacheHeader header{
                CACHE_MAGIC,
                CACHE_VERSION,
                0,
                identity.source_key,
                identity.source_size,
                identity.source_write_time
            };
            return
                write_value(output, header) &&
                write_vector(output, scene.vertices) &&
                write_vector(output, scene.indices) &&
                write_materials(output, scene.materials) &&
                write_vector(output, scene.submeshes) &&
                write_vector(output, scene.meshes) &&
                write_value(output, scene.root_node_id) &&
                write_nodes(output, scene.nodes) &&
                write_vector(output, scene.instances) &&
                write_vector(output, scene.draw_instances) &&
                write_vector(output, scene.draw_calls) &&
                write_value(output, scene.world_aabb);
        }

        std::unique_ptr<SceneCPUData> read_scene(
            const CacheIdentity& identity) {

            try {
                std::ifstream input(identity.cache_path, std::ios::binary);
                if (!input)
                    return nullptr;

                CacheHeader header{};
                if (!read_value(input, header) ||
                    header.magic != CACHE_MAGIC ||
                    header.version != CACHE_VERSION ||
                    header.source_key != identity.source_key ||
                    header.source_size != identity.source_size ||
                    header.source_write_time != identity.source_write_time) {
                    return nullptr;
                }

                auto scene = std::make_unique<SceneCPUData>();
                if (!read_vector(input, scene->vertices) ||
                    !read_vector(input, scene->indices) ||
                    !read_materials(input, scene->materials) ||
                    !read_vector(input, scene->submeshes) ||
                    !read_vector(input, scene->meshes) ||
                    !read_value(input, scene->root_node_id) ||
                    !read_nodes(input, scene->nodes) ||
                    !read_vector(input, scene->instances) ||
                    !read_vector(input, scene->draw_instances) ||
                    !read_vector(input, scene->draw_calls) ||
                    !read_value(input, scene->world_aabb)) {
                    return nullptr;
                }
                return scene;
            }
            catch (const std::exception&) {
                return nullptr;
            }
        }

        std::unique_ptr<SceneCPUData> build_scene_cpu(
            const util::ProgramArgument& argument) {

            auto source = SceneSourceFactory::create_scene(argument);
            return std::make_unique<SceneCPUData>(
                SceneCPUBuilder::build(*source));
        }
    }

    std::unique_ptr<SceneCPUData> load_or_build_scene_cpu(
        const util::ProgramArgument& argument) {

        CacheIdentity identity{};
        if (!make_identity(argument, identity))
            return build_scene_cpu(argument);

        if (std::unique_ptr<SceneCPUData> cached = read_scene(identity)) {
            util::Logger::g_logger << "Scene CPU cache hit: "
                << identity.cache_path.string() << '\n';
            return cached;
        }

        std::unique_ptr<SceneCPUData> scene = build_scene_cpu(argument);
        if (write_scene(identity.cache_path, identity, *scene)) {
            util::Logger::g_logger << "Scene CPU cache write: "
                << identity.cache_path.string() << '\n';
        }
        else {
            util::Logger::g_logger << "Scene CPU cache write failed: "
                << identity.cache_path.string() << '\n';
        }
        return scene;
    }

}
