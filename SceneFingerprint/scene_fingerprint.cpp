#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct Vec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Fingerprint
{
    std::filesystem::path path;
    uintmax_t file_size_bytes = 0;
    bool ok = false;
    std::string message;

    uint32_t mesh_count = 0;
    uint32_t material_count = 0;
    uint64_t vertex_count = 0;
    uint64_t index_count = 0;
    uint64_t triangle_count = 0;
    uint64_t face_count = 0;
    uint64_t degenerate_triangle_count = 0;

    Vec3 bounds_min = {DBL_MAX, DBL_MAX, DBL_MAX};
    Vec3 bounds_max = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
    double extent_x = 0.0;
    double extent_y = 0.0;
    double extent_z = 0.0;
    double diagonal = 0.0;
    double bounding_radius = 0.0;

    double surface_area = 0.0;
    double min_triangle_area = DBL_MAX;
    double max_triangle_area = 0.0;
    double avg_triangle_area = 0.0;
    uint64_t tiny_triangle_count = 0;
    uint64_t small_triangle_count = 0;
    uint64_t medium_triangle_count = 0;
    uint64_t large_triangle_count = 0;

    uint32_t draw_call_proxy = 0;
    uint64_t mesh_tri_min = 0;
    uint64_t mesh_tri_max = 0;
    double mesh_tri_avg = 0.0;

    uint32_t referenced_material_count = 0;
    uint64_t material_tri_min = 0;
    uint64_t material_tri_max = 0;
    double material_tri_avg = 0.0;
    double dominant_material_triangle_ratio = 0.0;
    uint32_t material_with_color_count = 0;

    uint32_t texture_reference_count = 0;
    uint32_t unique_texture_count = 0;
    uint32_t missing_texture_file_count = 0;
    uint32_t diffuse_texture_count = 0;
    uint32_t normal_texture_count = 0;
    uint32_t specular_metal_rough_texture_count = 0;
    uint32_t emissive_texture_count = 0;
    uint32_t opacity_texture_count = 0;

    double indices_per_vertex = 0.0;
    bool fits_uint16_indices = false;
    double triangle_density_bbox_surface = 0.0;
};

struct Options
{
    std::vector<std::filesystem::path> paths;
    std::optional<std::filesystem::path> inventory_path;
    std::optional<std::filesystem::path> output_path;
    bool all_inventory_paths = false;
};

Vec3 to_vec3(const aiVector3D& value)
{
    return {value.x, value.y, value.z};
}

Vec3 subtract(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 cross(Vec3 a, Vec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}

double length(Vec3 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

void include_bounds(Fingerprint& fingerprint, Vec3 position)
{
    fingerprint.bounds_min.x = std::min(fingerprint.bounds_min.x, position.x);
    fingerprint.bounds_min.y = std::min(fingerprint.bounds_min.y, position.y);
    fingerprint.bounds_min.z = std::min(fingerprint.bounds_min.z, position.z);
    fingerprint.bounds_max.x = std::max(fingerprint.bounds_max.x, position.x);
    fingerprint.bounds_max.y = std::max(fingerprint.bounds_max.y, position.y);
    fingerprint.bounds_max.z = std::max(fingerprint.bounds_max.z, position.z);
}

std::string csv_escape(const std::string& value)
{
    if (value.find_first_of(",\"\n\r") == std::string::npos) {
        return value;
    }

    std::string escaped = "\"";
    for (char c : value) {
        if (c == '"') {
            escaped += "\"\"";
        } else {
            escaped += c;
        }
    }
    escaped += "\"";
    return escaped;
}

std::string path_string(const std::filesystem::path& path)
{
    return path.generic_string();
}

uint64_t min_nonzero_or_zero(const std::vector<uint64_t>& values)
{
    if (values.empty()) {
        return 0;
    }
    return *std::min_element(values.begin(), values.end());
}

uint64_t max_or_zero(const std::vector<uint64_t>& values)
{
    if (values.empty()) {
        return 0;
    }
    return *std::max_element(values.begin(), values.end());
}

double average_or_zero(const std::vector<uint64_t>& values)
{
    if (values.empty()) {
        return 0.0;
    }
    const uint64_t sum = std::accumulate(values.begin(), values.end(), uint64_t{0});
    return static_cast<double>(sum) / static_cast<double>(values.size());
}

bool has_color(const aiMaterial* material)
{
    aiColor4D color = {};
    return AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_BASE_COLOR, &color) ||
           AI_SUCCESS == aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &color);
}

uint32_t collect_texture_type(
    const aiMaterial* material,
    aiTextureType type,
    const std::filesystem::path& model_directory,
    std::set<std::string>& unique_textures,
    uint32_t& missing_texture_file_count)
{
    const uint32_t count = material->GetTextureCount(type);
    for (uint32_t texture_index = 0; texture_index < count; ++texture_index) {
        aiString texture_path;
        if (material->GetTexture(type, texture_index, &texture_path) != AI_SUCCESS) {
            continue;
        }

        const std::string texture = texture_path.C_Str();
        unique_textures.insert(texture);
        if (!texture.empty() && texture[0] != '*') {
            std::filesystem::path resolved = model_directory / std::filesystem::path(texture);
            if (!std::filesystem::exists(resolved)) {
                ++missing_texture_file_count;
            }
        }
    }
    return count;
}

void collect_material_metrics(
    const aiScene* scene,
    const std::filesystem::path& model_directory,
    const std::map<uint32_t, uint64_t>& material_triangle_counts,
    Fingerprint& fingerprint)
{
    std::set<std::string> unique_textures;

    for (uint32_t material_index = 0; material_index < scene->mNumMaterials; ++material_index) {
        const aiMaterial* material = scene->mMaterials[material_index];
        if (material == nullptr) {
            continue;
        }

        if (has_color(material)) {
            ++fingerprint.material_with_color_count;
        }

        fingerprint.diffuse_texture_count += collect_texture_type(
            material,
            aiTextureType_DIFFUSE,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
        fingerprint.diffuse_texture_count += collect_texture_type(
            material,
            aiTextureType_BASE_COLOR,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
        fingerprint.normal_texture_count += collect_texture_type(
            material,
            aiTextureType_NORMALS,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
        fingerprint.specular_metal_rough_texture_count += collect_texture_type(
            material,
            aiTextureType_SPECULAR,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
        fingerprint.specular_metal_rough_texture_count += collect_texture_type(
            material,
            aiTextureType_METALNESS,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
        fingerprint.specular_metal_rough_texture_count += collect_texture_type(
            material,
            aiTextureType_DIFFUSE_ROUGHNESS,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
        fingerprint.emissive_texture_count += collect_texture_type(
            material,
            aiTextureType_EMISSIVE,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
        fingerprint.opacity_texture_count += collect_texture_type(
            material,
            aiTextureType_OPACITY,
            model_directory,
            unique_textures,
            fingerprint.missing_texture_file_count);
    }

    fingerprint.texture_reference_count =
        fingerprint.diffuse_texture_count +
        fingerprint.normal_texture_count +
        fingerprint.specular_metal_rough_texture_count +
        fingerprint.emissive_texture_count +
        fingerprint.opacity_texture_count;
    fingerprint.unique_texture_count = static_cast<uint32_t>(unique_textures.size());

    std::vector<uint64_t> material_triangles;
    material_triangles.reserve(material_triangle_counts.size());
    uint64_t dominant = 0;
    for (const auto& [material_index, triangle_count] : material_triangle_counts) {
        (void)material_index;
        if (triangle_count > 0) {
            material_triangles.push_back(triangle_count);
            dominant = std::max(dominant, triangle_count);
        }
    }

    fingerprint.referenced_material_count = static_cast<uint32_t>(material_triangles.size());
    fingerprint.material_tri_min = min_nonzero_or_zero(material_triangles);
    fingerprint.material_tri_max = max_or_zero(material_triangles);
    fingerprint.material_tri_avg = average_or_zero(material_triangles);
    if (fingerprint.triangle_count > 0) {
        fingerprint.dominant_material_triangle_ratio =
            static_cast<double>(dominant) / static_cast<double>(fingerprint.triangle_count);
    }
}

void finalize_geometry_metrics(Fingerprint& fingerprint, const std::vector<uint64_t>& mesh_triangle_counts)
{
    if (fingerprint.vertex_count == 0) {
        fingerprint.bounds_min = {};
        fingerprint.bounds_max = {};
    }

    fingerprint.extent_x = fingerprint.bounds_max.x - fingerprint.bounds_min.x;
    fingerprint.extent_y = fingerprint.bounds_max.y - fingerprint.bounds_min.y;
    fingerprint.extent_z = fingerprint.bounds_max.z - fingerprint.bounds_min.z;
    fingerprint.diagonal = length({fingerprint.extent_x, fingerprint.extent_y, fingerprint.extent_z});
    fingerprint.bounding_radius = fingerprint.diagonal * 0.5;

    if (fingerprint.triangle_count > 0) {
        fingerprint.avg_triangle_area =
            fingerprint.surface_area / static_cast<double>(fingerprint.triangle_count);
    } else {
        fingerprint.min_triangle_area = 0.0;
    }

    const double bbox_surface =
        2.0 * (fingerprint.extent_x * fingerprint.extent_y +
               fingerprint.extent_y * fingerprint.extent_z +
               fingerprint.extent_x * fingerprint.extent_z);
    if (bbox_surface > 0.0) {
        fingerprint.triangle_density_bbox_surface =
            static_cast<double>(fingerprint.triangle_count) / bbox_surface;
    }

    fingerprint.mesh_tri_min = min_nonzero_or_zero(mesh_triangle_counts);
    fingerprint.mesh_tri_max = max_or_zero(mesh_triangle_counts);
    fingerprint.mesh_tri_avg = average_or_zero(mesh_triangle_counts);
    fingerprint.draw_call_proxy = static_cast<uint32_t>(mesh_triangle_counts.size());
    fingerprint.indices_per_vertex =
        fingerprint.vertex_count > 0
            ? static_cast<double>(fingerprint.index_count) / static_cast<double>(fingerprint.vertex_count)
            : 0.0;
    fingerprint.fits_uint16_indices = fingerprint.vertex_count <= std::numeric_limits<uint16_t>::max();
}

Fingerprint analyze_scene(const std::filesystem::path& path)
{
    Fingerprint fingerprint;
    fingerprint.path = path;

    if (std::filesystem::exists(path)) {
        fingerprint.file_size_bytes = std::filesystem::file_size(path);
    } else {
        fingerprint.message = "File does not exist.";
        return fingerprint;
    }

    Assimp::Importer importer;
    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_GenSmoothNormals |
        aiProcess_ImproveCacheLocality |
        aiProcess_PreTransformVertices |
        aiProcess_ConvertToLeftHanded |
        aiProcess_SortByPType;

    const aiScene* scene = importer.ReadFile(path.string(), flags);
    if (scene == nullptr) {
        fingerprint.message = importer.GetErrorString();
        return fingerprint;
    }

    fingerprint.ok = true;
    fingerprint.message = "ok";
    fingerprint.mesh_count = scene->mNumMeshes;
    fingerprint.material_count = scene->mNumMaterials;

    std::vector<uint64_t> mesh_triangle_counts;
    std::map<uint32_t, uint64_t> material_triangle_counts;
    std::vector<double> triangle_areas;

    for (uint32_t mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
        const aiMesh* mesh = scene->mMeshes[mesh_index];
        if (mesh == nullptr) {
            continue;
        }

        fingerprint.vertex_count += mesh->mNumVertices;
        fingerprint.face_count += mesh->mNumFaces;

        for (uint32_t vertex_index = 0; vertex_index < mesh->mNumVertices; ++vertex_index) {
            include_bounds(fingerprint, to_vec3(mesh->mVertices[vertex_index]));
        }

        uint64_t mesh_triangles = 0;
        for (uint32_t face_index = 0; face_index < mesh->mNumFaces; ++face_index) {
            const aiFace& face = mesh->mFaces[face_index];
            fingerprint.index_count += face.mNumIndices;
            if (face.mNumIndices != 3) {
                continue;
            }

            const Vec3 p0 = to_vec3(mesh->mVertices[face.mIndices[0]]);
            const Vec3 p1 = to_vec3(mesh->mVertices[face.mIndices[1]]);
            const Vec3 p2 = to_vec3(mesh->mVertices[face.mIndices[2]]);
            const double area = 0.5 * length(cross(subtract(p1, p0), subtract(p2, p0)));

            ++mesh_triangles;
            ++fingerprint.triangle_count;
            fingerprint.surface_area += area;
            fingerprint.min_triangle_area = std::min(fingerprint.min_triangle_area, area);
            fingerprint.max_triangle_area = std::max(fingerprint.max_triangle_area, area);
            if (area <= 1.0e-12) {
                ++fingerprint.degenerate_triangle_count;
            }
            triangle_areas.push_back(area);
        }

        if (mesh_triangles > 0) {
            mesh_triangle_counts.push_back(mesh_triangles);
            material_triangle_counts[mesh->mMaterialIndex] += mesh_triangles;
        }
    }

    finalize_geometry_metrics(fingerprint, mesh_triangle_counts);

    const double reference_area = std::max(1.0e-12, fingerprint.diagonal * fingerprint.diagonal);
    for (double area : triangle_areas) {
        const double normalized = area / reference_area;
        if (normalized < 1.0e-8) {
            ++fingerprint.tiny_triangle_count;
        } else if (normalized < 1.0e-6) {
            ++fingerprint.small_triangle_count;
        } else if (normalized < 1.0e-4) {
            ++fingerprint.medium_triangle_count;
        } else {
            ++fingerprint.large_triangle_count;
        }
    }

    collect_material_metrics(scene, path.parent_path(), material_triangle_counts, fingerprint);
    return fingerprint;
}

std::vector<std::filesystem::path> parse_inventory(
    const std::filesystem::path& inventory_path,
    bool all_inventory_paths)
{
    std::ifstream input(inventory_path);
    std::vector<std::filesystem::path> paths;
    if (!input) {
        return paths;
    }

    bool in_preferred_section = false;
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("## ", 0) == 0) {
            in_preferred_section = line.find("Preferred Smoke-Test Entry Points") != std::string::npos;
            continue;
        }

        if (!all_inventory_paths && !in_preferred_section) {
            continue;
        }

        const std::string marker = "assets/scenes/";
        const std::size_t start = line.find(marker);
        if (start == std::string::npos) {
            continue;
        }

        std::size_t end = line.find(" (", start);
        if (end == std::string::npos) {
            end = line.size();
        }

        paths.emplace_back(line.substr(start, end - start));
    }

    std::sort(paths.begin(), paths.end());
    paths.erase(std::unique(paths.begin(), paths.end()), paths.end());
    return paths;
}

Options parse_options(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--inventory" && i + 1 < argc) {
            options.inventory_path = argv[++i];
        } else if (arg == "--all-inventory-paths") {
            options.all_inventory_paths = true;
        } else if (arg == "--output" && i + 1 < argc) {
            options.output_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout
                << "usage: scene_fingerprint [--inventory path] [--all-inventory-paths] "
                   "[--output path] [model ...]\n";
            std::exit(0);
        } else {
            options.paths.emplace_back(arg);
        }
    }

    if (options.inventory_path) {
        auto inventory_paths = parse_inventory(*options.inventory_path, options.all_inventory_paths);
        options.paths.insert(options.paths.end(), inventory_paths.begin(), inventory_paths.end());
    }

    std::sort(options.paths.begin(), options.paths.end());
    options.paths.erase(std::unique(options.paths.begin(), options.paths.end()), options.paths.end());
    return options;
}

void write_csv_header(std::ostream& output)
{
    output
        << "path,file_size_bytes,ok,message,mesh_count,material_count,vertex_count,index_count,"
        << "triangle_count,face_count,draw_call_proxy,bounds_min_x,bounds_min_y,bounds_min_z,"
        << "bounds_max_x,bounds_max_y,bounds_max_z,extent_x,extent_y,extent_z,diagonal,"
        << "bounding_radius,surface_area,min_triangle_area,max_triangle_area,avg_triangle_area,"
        << "degenerate_triangle_count,tiny_triangle_count,small_triangle_count,medium_triangle_count,"
        << "large_triangle_count,mesh_tri_min,mesh_tri_max,mesh_tri_avg,referenced_material_count,"
        << "material_tri_min,material_tri_max,material_tri_avg,dominant_material_triangle_ratio,"
        << "material_with_color_count,texture_reference_count,unique_texture_count,missing_texture_file_count,"
        << "diffuse_texture_count,normal_texture_count,specular_metal_rough_texture_count,"
        << "emissive_texture_count,opacity_texture_count,indices_per_vertex,fits_uint16_indices,"
        << "triangle_density_bbox_surface\n";
}

void write_csv_row(std::ostream& output, const Fingerprint& f)
{
    output << std::fixed << std::setprecision(6);
    output
        << csv_escape(path_string(f.path)) << ','
        << f.file_size_bytes << ','
        << (f.ok ? 1 : 0) << ','
        << csv_escape(f.message) << ','
        << f.mesh_count << ','
        << f.material_count << ','
        << f.vertex_count << ','
        << f.index_count << ','
        << f.triangle_count << ','
        << f.face_count << ','
        << f.draw_call_proxy << ','
        << f.bounds_min.x << ','
        << f.bounds_min.y << ','
        << f.bounds_min.z << ','
        << f.bounds_max.x << ','
        << f.bounds_max.y << ','
        << f.bounds_max.z << ','
        << f.extent_x << ','
        << f.extent_y << ','
        << f.extent_z << ','
        << f.diagonal << ','
        << f.bounding_radius << ','
        << f.surface_area << ','
        << f.min_triangle_area << ','
        << f.max_triangle_area << ','
        << f.avg_triangle_area << ','
        << f.degenerate_triangle_count << ','
        << f.tiny_triangle_count << ','
        << f.small_triangle_count << ','
        << f.medium_triangle_count << ','
        << f.large_triangle_count << ','
        << f.mesh_tri_min << ','
        << f.mesh_tri_max << ','
        << f.mesh_tri_avg << ','
        << f.referenced_material_count << ','
        << f.material_tri_min << ','
        << f.material_tri_max << ','
        << f.material_tri_avg << ','
        << f.dominant_material_triangle_ratio << ','
        << f.material_with_color_count << ','
        << f.texture_reference_count << ','
        << f.unique_texture_count << ','
        << f.missing_texture_file_count << ','
        << f.diffuse_texture_count << ','
        << f.normal_texture_count << ','
        << f.specular_metal_rough_texture_count << ','
        << f.emissive_texture_count << ','
        << f.opacity_texture_count << ','
        << f.indices_per_vertex << ','
        << (f.fits_uint16_indices ? 1 : 0) << ','
        << f.triangle_density_bbox_surface << '\n';
}
}

int main(int argc, char** argv)
{
    const Options options = parse_options(argc, argv);
    if (options.paths.empty()) {
        std::cerr << "scene_fingerprint: no input paths. Use --help for usage.\n";
        return 2;
    }

    std::ofstream file_output;
    std::ostream* output = &std::cout;
    if (options.output_path) {
        file_output.open(*options.output_path, std::ios::binary);
        if (!file_output) {
            std::cerr << "scene_fingerprint: failed to open output file: "
                      << path_string(*options.output_path) << "\n";
            return 2;
        }
        output = &file_output;
    }

    write_csv_header(*output);
    bool had_failure = false;
    for (const std::filesystem::path& path : options.paths) {
        Fingerprint fingerprint = analyze_scene(path);
        had_failure = had_failure || !fingerprint.ok;
        write_csv_row(*output, fingerprint);
    }

    return had_failure ? 1 : 0;
}
