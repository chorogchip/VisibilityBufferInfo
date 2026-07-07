#pragma once

#include "render/RendererBase.h"

namespace rndr {

	class RendererForward : public RendererBase {
	public:
		RendererForward() = default;
		~RendererForward() override = default;

		void init_() override;
		void render_() override;

	private:
		void create_dsv_heap() override;
		void create_rtv_heap() override;
		void create_render_targets() override;
		void create_depth_stencil_buffer() override;
		void create_root_signature() override;
		void create_pso() override;
	};
}