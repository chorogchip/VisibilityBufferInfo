#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "scene/SceneDataCPU.h"
#include "util/ProgramArgument.h"

namespace scene {

	class SceneFingerprint {
	public:
		struct Metrics {
			std::filesystem::path source_path;
			uintmax_t file_size_bytes = 0;
			bool ok = false;
			std::string message;

			uint32_t mesh_count = 0;
			uint32_t object_count = 0;
			uint32_t material_count = 0;
			uint64_t vertex_count = 0;
			uint64_t index_count = 0;
			uint64_t triangle_count = 0;
			uint64_t degenerate_triangle_count = 0;

			double bounds_min_x = 0.0;
			double bounds_min_y = 0.0;
			double bounds_min_z = 0.0;
			double bounds_max_x = 0.0;
			double bounds_max_y = 0.0;
			double bounds_max_z = 0.0;
			double extent_x = 0.0;
			double extent_y = 0.0;
			double extent_z = 0.0;
			double diagonal = 0.0;
			double bounding_radius = 0.0;

			double surface_area = 0.0;
			double min_triangle_area = 0.0;
			double max_triangle_area = 0.0;
			double avg_triangle_area = 0.0;
			uint64_t tiny_triangle_count = 0;
			uint64_t small_triangle_count = 0;
			uint64_t medium_triangle_count = 0;
			uint64_t large_triangle_count = 0;

			uint64_t mesh_tri_min = 0;
			uint64_t mesh_tri_max = 0;
			double mesh_tri_avg = 0.0;
			uint32_t referenced_material_count = 0;
			uint64_t material_tri_min = 0;
			uint64_t material_tri_max = 0;
			double material_tri_avg = 0.0;
			double dominant_material_triangle_ratio = 0.0;

			uint32_t texture_reference_count = 0;
			uint32_t unique_texture_count = 0;
			uint32_t missing_texture_file_count = 0;
			double indices_per_vertex = 0.0;
			bool fits_uint16_indices = false;
			double triangle_density_bbox_surface = 0.0;
		};

		static void write_csv(
			const std::filesystem::path& path,
			const SceneDataCPU& scene,
			const util::ProgramArgument& arg);

	private:
		static Metrics analyze(const SceneDataCPU& scene);
	};

}
