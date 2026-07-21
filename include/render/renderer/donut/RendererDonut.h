#pragma once

#include "render/renderer/RendererBase.h"

namespace rndr {
	class RendererDonut : public RendererBase {

	public:
		virtual ~RendererDonut() = 0;

	protected:
		void init1_() override;
		void render_prepare_() override;
		virtual void init2_() = 0;

	};
}
