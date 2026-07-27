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
#include "scene/data/gpu/DonutSceneGPUData.h"

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

		std::unique_ptr<scene::SceneCPUData> scene_cpu_;
		scene::SceneCPUDrawStream draw_stream_;
		std::unique_ptr<scene::DonutSceneGPUData> scene_gpu_;

	private:
		void create_draw_instance_id_upload_buffers();
		void update_draw_instance_id_buffer();

		std::array<
			Microsoft::WRL::ComPtr<ID3D12Resource>,
			util::Constants::FRAME_COUNT> draw_instance_id_upload_buffers_;
		bool draw_stream_dirty_ = false;

	};
}
