#pragma once

#include <array>
#include <memory>
#include <wrl.h>

#include <d3d12.h>

#include "engine/GPUResource.h"
#include "render/renderer/RendererBase.h"

#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "scene/data/cpu/SceneCPUData.h"
#include "scene/data/cpu/SceneCPUDrawStream.h"
#include "scene/data/cpu/JungleSceneCPUData.h"
#include "scene/data/cpu/JungleSceneCPUDrawStream.h"
#include "scene/data/gpu/DonutSceneGPUData.h"
#include "scene/data/gpu/JungleSceneGPUData.h"

namespace rndr {
	class RendererDonut : public RendererBase {

	public:
		virtual ~RendererDonut() = default;

	protected:
		void init1_() override;
		void render_prepare_() override;
		void render_update_scene_resources_() override;
		virtual void init2_() = 0;
		virtual void render_prepare_donut_() = 0;

		eng::ResourceManagerFrame resource_manager_frame_;
		eng::ResourceManagerSampler resource_manager_sampler_;
		eng::ResourceManagerShader resource_manager_shader_;
		eng::GPUResource depth_stencil_buffer_;

		std::unique_ptr<scene::SceneCPUData> scene_cpu_storage_;
		std::unique_ptr<scene::JungleSceneCPUData>
			jungle_scene_cpu_;
		scene::SceneCPUData* scene_cpu_ = nullptr;
		scene::SceneCPUDrawStream draw_stream_;
		scene::JungleSceneCPUDrawStream jungle_draw_stream_;
		std::unique_ptr<scene::DonutSceneGPUData> scene_gpu_;
		std::unique_ptr<scene::JungleSceneGPUData>
			jungle_scene_gpu_;

	private:
		void create_draw_instance_id_upload_buffers();
		void update_draw_instance_id_buffer();
		void create_jungle_point_id_upload_buffers();
		void update_jungle_point_id_buffer();

		std::array<
			Microsoft::WRL::ComPtr<ID3D12Resource>,
			util::Constants::FRAME_COUNT> draw_instance_id_upload_buffers_;
		std::array<
			Microsoft::WRL::ComPtr<ID3D12Resource>,
			util::Constants::FRAME_COUNT>
				jungle_point_id_upload_buffers_;
		bool draw_stream_dirty_ = false;
		bool jungle_draw_stream_dirty_ = false;

	};
}
