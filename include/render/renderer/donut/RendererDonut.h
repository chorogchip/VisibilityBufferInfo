#pragma once

#include "render/renderer/RendererBase.h"

namespace rndr {
	class RendererDonut : public RendererBase {

	public:
		virtual ~RendererDonut() = 0;

	protected:
		void init_shader_resources_() override;

	};
}
