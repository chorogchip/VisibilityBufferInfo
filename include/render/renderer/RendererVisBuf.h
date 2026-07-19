#pragma once

#include <vector>

#include "render/renderer/RendererBase.h"
#include "render/pass/PassVisibility.h"
#include "render/pass/PassVisBufResolve.h"

namespace rndr {

	class RendererVisBuf : public RendererBase {
	public:
		RendererVisBuf() = default;
		~RendererVisBuf() override = default;

		void create_renderer_resources() override;
		void render_() override;

	private:
		void make_programresult(util::ProgramResult& result) override;

		void init_passes() override;

		Microsoft::WRL::ComPtr<ID3D12Resource> vis_buffer_;
		Microsoft::WRL::ComPtr<ID3D12Resource> mesh_buffer_;
		PassVisibility pass_visibility_;
		PassVisBufResolve pass_resolve_;
	};
}
