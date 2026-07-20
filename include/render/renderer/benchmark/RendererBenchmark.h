#pragma once

#include <vector>

#include "render/renderer/RendererBase.h"
#include "scene/SceneDataCPU.h"
#include "scene/SceneDataGPU.h"

namespace rndr {
	class RendererBenchmark : public RendererBase {

    public:
		virtual ~RendererBenchmark() = default;

	protected:
		void init_scene_() override;
		void init_shader_resources_() override;
		void init_renderer_resources_() override;

		[[nodiscard]] const util::ProgramArgument& benchmark_program_arguments() const {
			return benchmark_program_arguments_;
		}

		[[nodiscard]] const std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>&
			material_textures() const;

	private:
		void load_scene_();
		void create_scene_resources_();
		void create_benchmark_resources_();
		void create_sampler_descriptor_();
		[[nodiscard]] UINT material_texture_count_() const;

	protected:
		std::unique_ptr<scene::SceneDataCPU> scene_cpu_;
		std::unique_ptr<scene::SceneDataGPU> scene_gpu_;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> dummy_textures_;
		util::ProgramArgument benchmark_program_arguments_{};

	};
}
