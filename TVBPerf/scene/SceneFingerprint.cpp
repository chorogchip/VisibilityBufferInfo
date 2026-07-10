#include "SceneFingerprint.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <vector>

namespace scene {

	namespace {

		struct Vec3 {
			double x = 0.0;
			double y = 0.0;
			double z = 0.0;
		};

		static Vec3 to_vec3(const DirectX::XMFLOAT3& value) {
			return { value.x, value.y, value.z };
		}

		static Vec3 subtract(Vec3 a, Vec3 b) {
			return { a.x - b.x, a.y - b.y, a.z - b.z };
		}

		static Vec3 cross(Vec3 a, Vec3 b) {
			return {
				a.y * b.z - a.z * b.y,
				a.z * b.x - a.x * b.z,
				a.x * b.y - a.y * b.x
			};
		}

		static double length(Vec3 value) {
			return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
		}

		static uint64_t min_or_zero(const std::vector<uint64_t>& values) {
			if (values.empty()) return 0;
			return *std::min_element(values.begin(), values.end());
		}

		static uint64_t max_or_zero(const std::vector<uint64_t>& values) {
			if (values.empty()) return 0;
			return *std::max_element(values.begin(), values.end());
		}

		static double average_or_zero(const std::vector<uint64_t>& values) {
			if (values.empty()) return 0.0;
			const uint64_t sum = std::accumulate(values.begin(), values.end(), uint64_t{ 0 });
			return static_cast<double>(sum) / static_cast<double>(values.size());
		}

		static std::string path_string(const std::filesystem::path& path) {
			return path.generic_string();
		}

		static bool valid_triangle_indices(
			const SceneDataCPU& scene,
			uint32_t i0,
			uint32_t i1,
			uint32_t i2) {
			const size_t vertex_count = scene.vertices.size();
			return i0 < vertex_count && i1 < vertex_count && i2 < vertex_count;
		}

		static void write_header(std::ostream& output) {
			output
				<< util::ProgramArgument::get_header_string() << ','
				<< "scene_source_path,scene_file_size_bytes,scene_ok,scene_message,"
				<< "scene_mesh_count,scene_object_count,scene_material_count,"
				<< "scene_vertex_count,scene_index_count,scene_triangle_count,"
				<< "scene_degenerate_triangle_count,"
				<< "scene_bounds_min_x,scene_bounds_min_y,scene_bounds_min_z,"
				<< "scene_bounds_max_x,scene_bounds_max_y,scene_bounds_max_z,"
				<< "scene_extent_x,scene_extent_y,scene_extent_z,scene_diagonal,"
				<< "scene_bounding_radius,scene_surface_area,scene_min_triangle_area,"
				<< "scene_max_triangle_area,scene_avg_triangle_area,"
				<< "scene_tiny_triangle_count,scene_small_triangle_count,"
				<< "scene_medium_triangle_count,scene_large_triangle_count,"
				<< "scene_mesh_tri_min,scene_mesh_tri_max,scene_mesh_tri_avg,"
				<< "scene_referenced_material_count,scene_material_tri_min,"
				<< "scene_material_tri_max,scene_material_tri_avg,"
				<< "scene_dominant_material_triangle_ratio,"
				<< "scene_texture_reference_count,scene_unique_texture_count,"
				<< "scene_missing_texture_file_count,scene_indices_per_vertex,"
				<< "scene_fits_uint16_indices,scene_triangle_density_bbox_surface\n";
		}

		static void write_row(
			std::ostream& output,
			const util::ProgramArgument& arg,
			const SceneFingerprint::Metrics& f) {
			output << std::fixed << std::setprecision(6);
			output
				<< arg.to_string() << ','
				<< path_string(f.source_path) << ','
				<< f.file_size_bytes << ','
				<< (f.ok ? 1 : 0) << ','
				<< f.message << ','
				<< f.mesh_count << ','
				<< f.object_count << ','
				<< f.material_count << ','
				<< f.vertex_count << ','
				<< f.index_count << ','
				<< f.triangle_count << ','
				<< f.degenerate_triangle_count << ','
				<< f.bounds_min_x << ','
				<< f.bounds_min_y << ','
				<< f.bounds_min_z << ','
				<< f.bounds_max_x << ','
				<< f.bounds_max_y << ','
				<< f.bounds_max_z << ','
				<< f.extent_x << ','
				<< f.extent_y << ','
				<< f.extent_z << ','
				<< f.diagonal << ','
				<< f.bounding_radius << ','
				<< f.surface_area << ','
				<< f.min_triangle_area << ','
				<< f.max_triangle_area << ','
				<< f.avg_triangle_area << ','
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
				<< f.texture_reference_count << ','
				<< f.unique_texture_count << ','
				<< f.missing_texture_file_count << ','
				<< f.indices_per_vertex << ','
				<< (f.fits_uint16_indices ? 1 : 0) << ','
				<< f.triangle_density_bbox_surface << '\n';
		}

	}

	SceneFingerprint::Metrics SceneFingerprint::analyze(const SceneDataCPU& scene) {
		Metrics metrics{};
		metrics.source_path = scene.source_path;
		metrics.ok = scene.loaded;
		//metrics.message = scene.error_message; for no csv argument support for arbitrary string
		metrics.message = "no-message";
		metrics.mesh_count = static_cast<uint32_t>(scene.meshes.size());
		metrics.object_count = static_cast<uint32_t>(scene.objects.size());
		metrics.material_count = static_cast<uint32_t>(scene.materials.size());
		metrics.vertex_count = static_cast<uint64_t>(scene.vertices.size());
		metrics.index_count = static_cast<uint64_t>(scene.indices.size());
		metrics.triangle_count = metrics.index_count / 3;

		if (std::filesystem::exists(scene.source_path)) {
			metrics.file_size_bytes = std::filesystem::file_size(scene.source_path);
		}

		if (metrics.vertex_count > 0) {
			metrics.bounds_min_x = scene.bounds_min.x;
			metrics.bounds_min_y = scene.bounds_min.y;
			metrics.bounds_min_z = scene.bounds_min.z;
			metrics.bounds_max_x = scene.bounds_max.x;
			metrics.bounds_max_y = scene.bounds_max.y;
			metrics.bounds_max_z = scene.bounds_max.z;
		}

		metrics.extent_x = metrics.bounds_max_x - metrics.bounds_min_x;
		metrics.extent_y = metrics.bounds_max_y - metrics.bounds_min_y;
		metrics.extent_z = metrics.bounds_max_z - metrics.bounds_min_z;
		metrics.diagonal = length({ metrics.extent_x, metrics.extent_y, metrics.extent_z });
		metrics.bounding_radius = metrics.diagonal * 0.5;

		std::vector<uint64_t> mesh_triangle_counts;
		std::vector<double> triangle_areas;
		mesh_triangle_counts.reserve(scene.meshes.size());
		triangle_areas.reserve(static_cast<size_t>(metrics.triangle_count));

		for (const auto& mesh : scene.meshes) {
			const uint64_t mesh_triangles = mesh.index_count / 3;
			if (mesh_triangles > 0) {
				mesh_triangle_counts.push_back(mesh_triangles);
			}

			for (uint32_t offset = 0; offset + 2 < mesh.index_count; offset += 3) {
				const uint32_t index_pos = mesh.index_start + offset;
				if (index_pos + 2 >= scene.indices.size()) continue;

				const uint32_t i0 = scene.indices[index_pos + 0];
				const uint32_t i1 = scene.indices[index_pos + 1];
				const uint32_t i2 = scene.indices[index_pos + 2];
				if (!valid_triangle_indices(scene, i0, i1, i2)) continue;

				const Vec3 p0 = to_vec3(scene.vertices[i0].position);
				const Vec3 p1 = to_vec3(scene.vertices[i1].position);
				const Vec3 p2 = to_vec3(scene.vertices[i2].position);
				const double area = 0.5 * length(cross(subtract(p1, p0), subtract(p2, p0)));
				metrics.surface_area += area;
				if (triangle_areas.empty()) {
					metrics.min_triangle_area = area;
				} else {
					metrics.min_triangle_area = std::min(metrics.min_triangle_area, area);
				}
				metrics.max_triangle_area = std::max(metrics.max_triangle_area, area);
				if (area <= 1.0e-12) {
					++metrics.degenerate_triangle_count;
				}
				triangle_areas.push_back(area);
			}
		}

		if (!triangle_areas.empty()) {
			metrics.avg_triangle_area =
				metrics.surface_area / static_cast<double>(triangle_areas.size());
		}

		const double reference_area = std::max(1.0e-12, metrics.diagonal * metrics.diagonal);
		for (double area : triangle_areas) {
			const double normalized = area / reference_area;
			if (normalized < 1.0e-8) {
				++metrics.tiny_triangle_count;
			} else if (normalized < 1.0e-6) {
				++metrics.small_triangle_count;
			} else if (normalized < 1.0e-4) {
				++metrics.medium_triangle_count;
			} else {
				++metrics.large_triangle_count;
			}
		}

		metrics.mesh_tri_min = min_or_zero(mesh_triangle_counts);
		metrics.mesh_tri_max = max_or_zero(mesh_triangle_counts);
		metrics.mesh_tri_avg = average_or_zero(mesh_triangle_counts);

		std::map<uint32_t, uint64_t> material_triangle_counts;
		for (const auto& object : scene.objects) {
			if (object.mesh_index >= scene.meshes.size()) continue;
			const auto& mesh = scene.meshes[object.mesh_index];
			material_triangle_counts[object.material_index] += mesh.index_count / 3;
		}

		std::vector<uint64_t> material_triangles;
		uint64_t dominant = 0;
		for (const auto& item : material_triangle_counts) {
			if (item.second == 0) continue;
			material_triangles.push_back(item.second);
			dominant = std::max(dominant, item.second);
		}

		metrics.referenced_material_count = static_cast<uint32_t>(material_triangles.size());
		metrics.material_tri_min = min_or_zero(material_triangles);
		metrics.material_tri_max = max_or_zero(material_triangles);
		metrics.material_tri_avg = average_or_zero(material_triangles);
		if (metrics.triangle_count > 0) {
			metrics.dominant_material_triangle_ratio =
				static_cast<double>(dominant) / static_cast<double>(metrics.triangle_count);
		}

		std::set<std::string> unique_textures;
		for (const auto& material : scene.materials) {
			metrics.texture_reference_count += static_cast<uint32_t>(material.texture_paths.size());
			for (const auto& texture_path : material.texture_paths) {
				unique_textures.insert(path_string(texture_path));
				if (!texture_path.empty() && texture_path.string()[0] != '*') {
					const std::filesystem::path resolved = scene.source_path.parent_path() / texture_path;
					if (!std::filesystem::exists(resolved)) {
						++metrics.missing_texture_file_count;
					}
				}
			}
		}
		metrics.unique_texture_count = static_cast<uint32_t>(unique_textures.size());

		if (metrics.vertex_count > 0) {
			metrics.indices_per_vertex =
				static_cast<double>(metrics.index_count) / static_cast<double>(metrics.vertex_count);
		}
		metrics.fits_uint16_indices = metrics.vertex_count <= std::numeric_limits<uint16_t>::max();

		const double bbox_surface =
			2.0 * (metrics.extent_x * metrics.extent_y +
				metrics.extent_y * metrics.extent_z +
				metrics.extent_x * metrics.extent_z);
		if (bbox_surface > 0.0) {
			metrics.triangle_density_bbox_surface =
				static_cast<double>(metrics.triangle_count) / bbox_surface;
		}

		return metrics;
	}

	void SceneFingerprint::write_csv(
		const std::filesystem::path& path,
		const SceneDataCPU& scene,
		const util::ProgramArgument& arg) {
		const Metrics metrics = analyze(scene);

		std::ofstream output(path, std::ios::out | std::ios::trunc);
		if (!output) {
			std::cerr << "Failed to open scene fingerprint CSV: " << path.string() << '\n';
			return;
		}

		write_header(output);
		write_row(output, arg, metrics);

		if (!output) {
			std::cerr << "Failed to write scene fingerprint CSV: " << path.string() << '\n';
			return;
		}

		std::cout << "Saved scene fingerprint CSV: " << path.string() << '\n';
	}

}
