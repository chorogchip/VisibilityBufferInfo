#pragma once

#include <vector>

#include "render/renderer/RendererBase.h"
#include "render/pass/PassVisibility.h"
#include "render/pass/PassVisBufGBuffer.h"
#include "render/pass/PassDeferredLighting.h"

namespace rndr {

	class RendererVisBufGBuffer : public RendererBase {
	public:
		RendererVisBufGBuffer() = default;
		~RendererVisBufGBuffer() override = default;

	private:
		void render_() override;
		void make_programresult(util::ProgramResult& result) override;
		void create_pass_resources() override;

		void init_passes() override;

		Microsoft::WRL::ComPtr<ID3D12Resource> vis_buffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> mesh_buffer_;
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> gbuffers_;
		PassVisibility pass_visibility_;
		PassVisBufGBuffer pass_gbuffer_;
		PassDeferredLighting pass_lighting_;
	};
}
