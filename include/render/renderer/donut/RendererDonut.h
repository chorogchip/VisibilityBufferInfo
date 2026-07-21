#pragma once

#include "engine/GPUResource.h"
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

		std::unique_ptr<scene::DonutSceneDataCPU> scene_cpu_;
		std::unique_ptr<scene::DonutSceneDataGPU> scene_gpu_;

	};
}
