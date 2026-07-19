#include "render/camera/CameraPathController.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace rndr {
	void CameraPathController::init(const util::ProgramArgument& argument) {
		mode_ = argument.camera_mode;
		filepath_ = argument.camera_filepath;
		warmup_frames_ = argument.warmup_frames;
		default_measurement_frames_ = argument.measure_frames;
		keyframe_interval_ = argument.camera_keyframe_interval;
		if (is_playback()) path_.load_csv(filepath_);
	}

	void CameraPathController::before_render(Camera& camera) {
		if (mode_ == util::ProgramArgument::CAMERA_MODE_RECORD) {
			if (render_frame_ % keyframe_interval_ == 0) {
				const CameraPose pose = camera.get_pose();
				if (pose_changed(pose)) {
					path_.add_keyframe(render_frame_, pose);
					last_recorded_pose_ = pose;
				}
			}
			return;
		}

		if (!is_playback()) return;

		uint64_t playback_frame = 0;
		if (render_frame_ >= warmup_frames_)
			playback_frame = render_frame_ - warmup_frames_;
		playback_frame = std::min(playback_frame, path_.end_frame());
		camera.set_pose(path_.sample(playback_frame));
	}

	void CameraPathController::after_render() {
		++render_frame_;
	}

	void CameraPathController::close(const Camera& camera) {
		if (mode_ != util::ProgramArgument::CAMERA_MODE_RECORD) return;
		if (path_.empty() || path_.end_frame() < render_frame_)
			path_.add_keyframe(render_frame_, camera.get_pose());
		path_.save_csv(filepath_);
	}

	uint64_t CameraPathController::measurement_frames() const {
		return is_playback() ? path_.end_frame() + 1 : default_measurement_frames_;
	}

	bool CameraPathController::pose_changed(const CameraPose& pose) const {
		if (!last_recorded_pose_) return true;
		constexpr float POSITION_EPSILON_SQ = 0.000001f;
		constexpr float ROTATION_DOT_EPSILON = 0.999999f;
		const auto lhs_position = DirectX::XMLoadFloat3(&last_recorded_pose_->position);
		const auto rhs_position = DirectX::XMLoadFloat3(&pose.position);
		const float position_delta_sq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(
			DirectX::XMVectorSubtract(lhs_position, rhs_position)));
		const float rotation_dot = std::abs(DirectX::XMVectorGetX(DirectX::XMVector4Dot(
			DirectX::XMLoadFloat4(&last_recorded_pose_->rotation),
			DirectX::XMLoadFloat4(&pose.rotation))));
		return position_delta_sq > POSITION_EPSILON_SQ || rotation_dot < ROTATION_DOT_EPSILON;
	}
}
