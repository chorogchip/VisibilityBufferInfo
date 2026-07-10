#pragma once

#include <memory>

#include "util/ProgramArgument.h"
#include "util/FrameCounter.h"

#include "scene/SceneDataCPU.h"

#include "render/Camera.h"

namespace rndr {

	class ProcedureBase {

	public:
		ProcedureBase() = default;
		virtual ~ProcedureBase() = default;

		void init(util::ProgramArgument args);
		void close();

		bool to_terminate() const { return frame_counter_.to_terminate(); }

		rndr::Camera camera_{};

	protected:
		util::ProgramArgument program_arguments_;
		util::FrameCounter frame_counter_;

		std::unique_ptr<scene::SceneDataCPU> scene_cpu_;
	};
}