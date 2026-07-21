#pragma once

#include <vector>

#include "engine/GPUResource.h"
#include "engine/ResourceManagerFrame.h"
#include "engine/ResourceManagerSampler.h"
#include "engine/ResourceManagerShader.h"
#include "render/renderer/RendererBase.h"
#include "scene/SceneDataCPU.h"
#include "scene/SceneDataGPU.h"

namespace rndr {
	class RendererBenchmark : public RendererBase {

    public:
		virtual ~RendererBenchmark() = default;

	protected:
		void init1_() override;
		void render_prepare_() override;
		virtual void init2_() = 0;

		eng::ResourceManagerFrame resource_manager_frame_;
		eng::ResourceManagerSampler resource_manager_sampler_;
		eng::ResourceManagerShader resource_manager_shader_;
		eng::GPUResource depth_stencil_buffer_;

		std::unique_ptr<scene::SceneDataCPU> scene_cpu_;
		std::unique_ptr<scene::SceneDataGPU> scene_gpu_;
		eng::GPUResource scene_vertex_buffer_;
		eng::GPUResource scene_index_buffer_;
		eng::GPUResource scene_object_buffer_;
		eng::GPUResource scene_material_buffer_;
		eng::GPUResource scene_mesh_buffer_;
		std::vector<eng::GPUResource> textures_;

		struct ConstBufMatrices {
			DirectX::XMFLOAT4X4 mat_view_;
			DirectX::XMFLOAT4X4 mat_proj_;
			DirectX::XMFLOAT2 viewport_size_;
		};
		dxutl::UploadConstBuf<ConstBufMatrices> buf_constant_[util::Constants::FRAME_COUNT];

	private:
		void wrap_scene_resources();
		void create_dummy_textures();
	};
}
