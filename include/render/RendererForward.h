#pragma once

#include "render/RendererBase.h"

namespace rndr {

	class RendererForward : public RendererBase {
	public:
		RendererForward() = default;
		~RendererForward() override = default;

		void render_() override;

	private:
		virtual void make_programresult(util::ProgramResult& result) override;

		UINT srv_descriptor_count() const override;
		void create_shader_resources() override;
		void create_root_signature() override;
		void create_pso() override;
	};
}
