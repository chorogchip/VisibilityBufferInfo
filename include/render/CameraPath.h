#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "render/Camera.h"

namespace rndr {

	struct CameraKeyframe {
		uint64_t frame = 0;
		CameraPose pose{};
	};

	class CameraPath {
	public:
		void load_csv(const std::filesystem::path& path);
		void save_csv(const std::filesystem::path& path) const;
		void add_keyframe(uint64_t frame, const CameraPose& pose);

		CameraPose sample(uint64_t frame) const;
		uint64_t end_frame() const;
		bool empty() const { return keyframes_.empty(); }

	private:
		std::vector<CameraKeyframe> keyframes_;
	};
}
