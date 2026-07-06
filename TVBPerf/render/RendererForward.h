#pragma once

#include "render/RendererBase.h"

namespace rndr {

	class RendererForward : public RendererBase {
	public:
		RendererForward() = default;
		~RendererForward() override = default;

		void render() override;

	private:
		void create_root_signature() override;
		void create_pso() override;

	private:
	};
}