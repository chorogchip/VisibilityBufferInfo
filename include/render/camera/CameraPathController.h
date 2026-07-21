#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "ProgramArgument.h"
#include "render/camera/Camera.h"
#include "render/camera/CameraPath.h"

namespace rndr {
	class CameraPathController {
	public:
		void init(const util::ProgramArgument& argument);
		void before_render(Camera& camera);
		void after_render();
		void close(const Camera& camera);

		uint64_t measurement_frames() const;
		bool is_playback() const { return mode_ == 1; }

	private:
		bool pose_changed(const CameraPose& pose) const;

		uint32_t mode_ = 0;
		std::string filepath_{};
		uint64_t warmup_frames_ = 0;
		uint64_t default_measurement_frames_ = 0;
		uint64_t keyframe_interval_ = 10;
		uint64_t render_frame_ = 0;
		CameraPath path_{};
		std::optional<CameraPose> last_recorded_pose_{};
	};
}
