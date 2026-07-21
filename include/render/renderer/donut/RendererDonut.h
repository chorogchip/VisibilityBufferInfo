#pragma once

#include <wrl.h>

#include "engine/GPUResource.h"
#include "engine/GraphicsQueue.h"
#include "render/renderer/RendererBase.h"

#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "scene/donut/DonutSceneDataCPU.h"
#include "scene/donut/DonutSceneDataGPU.h"

namespace rndr {
	class RendererDonut : public RendererBase {

	public:
		virtual ~RendererDonut() = 0;

	protected:
		void init1_() override;
		void render_prepare_() override;
		virtual void init2_() = 0;

		eng::ResourceManagerFrame resource_manager_frame_;
		eng::ResourceManagerSampler resource_manager_sampler_;
		eng::ResourceManagerShader resource_manager_shader_;
		eng::GPUResource depth_stencil_buffer_;

		eng::GraphicsQueue compute_queue_;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> compute_command_allocator_[util::Constants::FRAME_COUNT];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> compute_command_list_;

		std::unique_ptr<scene::DonutSceneDataCPU> scene_cpu_;
		std::unique_ptr<scene::DonutSceneDataGPU> scene_gpu_;

	};
}
