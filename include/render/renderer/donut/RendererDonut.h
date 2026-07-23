#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include <d3d12.h>
#include <wrl.h>

#include "engine/GPUResource.h"
#include "engine/GPUQueue.h"
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
		virtual void render_prepare_donut_() = 0;
		void record_render_instance_upload_(ID3D12GraphicsCommandList* command_list);

		eng::ResourceManagerFrame resource_manager_frame_;
		eng::ResourceManagerSampler resource_manager_sampler_;
		eng::ResourceManagerShader resource_manager_shader_;
		eng::GPUResource depth_stencil_buffer_;

		eng::GPUQueue compute_queue_;
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> compute_command_allocator_[util::Constants::FRAME_COUNT];
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> compute_command_list_;

		std::unique_ptr<scene::DonutSceneDataCPU> scene_cpu_;
		std::unique_ptr<scene::DonutSceneDataGPU> scene_gpu_;
		std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,
			util::Constants::FRAME_COUNT> render_instance_upload_buffers_;
		std::array<size_t, util::Constants::FRAME_COUNT>
			render_instance_upload_sizes_{};
		std::vector<scene::DonutSceneDataCPU::VisibleDraw> visible_draws_;

	};
}
