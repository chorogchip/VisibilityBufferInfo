#include "render/CameraPath.h"

#include <DirectXMath.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

#include "util/Logger.h"

using namespace DirectX;

static constexpr const char* CAMERA_PATH_HEADER = "frame,px,py,pz,qx,qy,qz,qw";

static std::vector<std::string> split_csv_line(const std::string& line) {
	std::vector<std::string> values;
	std::stringstream stream(line);
	std::string value;
	while (std::getline(stream, value, ',')) values.push_back(value);
	return values;
}

template <typename T>
static T parse_number(const std::string& text, const char* field) {
	T value{};
	const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec != std::errc{} || ptr != text.data() + text.size())
		util::Logger::g_logger << "invalid camera path field " << field << ": " << text << '\n';
	util::Logger::g_logger.assert_with_log(
		ec == std::errc{} && ptr == text.data() + text.size(),
		"invalid camera path number");
	return value;
}

static rndr::CameraPose normalized_pose(const rndr::CameraPose& input) {
	rndr::CameraPose result = input;
	XMVECTOR q = XMLoadFloat4(&result.rotation);
	const float length_sq = XMVectorGetX(XMVector4LengthSq(q));
	util::Logger::g_logger.assert_with_log(
		std::isfinite(length_sq) && length_sq > 0.0000001f,
		"camera path quaternion must be finite and non-zero");
	XMStoreFloat4(&result.rotation, XMQuaternionNormalize(q));
	return result;
}

namespace rndr {
	void CameraPath::load_csv(const std::filesystem::path& path) {
		std::ifstream input(path);
		if (!input) util::Logger::g_logger << "failed to open camera path: " << path.string() << '\n';
		util::Logger::g_logger.assert_with_log(
			static_cast<bool>(input), "failed to open camera path");

		std::string line;
		util::Logger::g_logger.assert_with_log(
			static_cast<bool>(std::getline(input, line)) && line == CAMERA_PATH_HEADER,
			"invalid camera path header");

		keyframes_.clear();
		while (std::getline(input, line)) {
			if (line.empty()) continue;
			const auto values = split_csv_line(line);
			util::Logger::g_logger.assert_with_log(
				values.size() == 8, "camera path row must have 8 columns");

			CameraPose pose{};
			const uint64_t frame = parse_number<uint64_t>(values[0], "frame");
			pose.position = XMFLOAT3(
				parse_number<float>(values[1], "px"),
				parse_number<float>(values[2], "py"),
				parse_number<float>(values[3], "pz"));
			pose.rotation = XMFLOAT4(
				parse_number<float>(values[4], "qx"),
				parse_number<float>(values[5], "qy"),
				parse_number<float>(values[6], "qz"),
				parse_number<float>(values[7], "qw"));
			add_keyframe(frame, pose);
		}

		util::Logger::g_logger.assert_with_log(!keyframes_.empty(), "camera path is empty");
		util::Logger::g_logger.assert_with_log(keyframes_.front().frame == 0,
			"camera path must start at frame 0");
	}

	void CameraPath::save_csv(const std::filesystem::path& path) const {
		std::ofstream output(path, std::ios::out | std::ios::trunc);
		if (!output) util::Logger::g_logger << "failed to write camera path: " << path.string() << '\n';
		util::Logger::g_logger.assert_with_log(
			static_cast<bool>(output), "failed to write camera path");
		output << CAMERA_PATH_HEADER << '\n' << std::fixed << std::setprecision(7);
		for (const auto& key : keyframes_) {
			output << key.frame << ','
				<< key.pose.position.x << ',' << key.pose.position.y << ',' << key.pose.position.z << ','
				<< key.pose.rotation.x << ',' << key.pose.rotation.y << ','
				<< key.pose.rotation.z << ',' << key.pose.rotation.w << '\n';
		}
		util::Logger::g_logger.assert_with_log(static_cast<bool>(output), "failed to save camera path");
	}

	void CameraPath::add_keyframe(uint64_t frame, const CameraPose& pose) {
		util::Logger::g_logger.assert_with_log(
			keyframes_.empty() || keyframes_.back().frame < frame,
			"camera path frames must be strictly increasing");
		keyframes_.push_back(CameraKeyframe{ frame, normalized_pose(pose) });
	}

	CameraPose CameraPath::sample(uint64_t frame) const {
		util::Logger::g_logger.assert_with_log(!keyframes_.empty(), "cannot sample an empty camera path");
		if (frame <= keyframes_.front().frame) return keyframes_.front().pose;
		if (frame >= keyframes_.back().frame) return keyframes_.back().pose;

		auto upper = std::upper_bound(keyframes_.begin(), keyframes_.end(), frame,
			[](uint64_t value, const CameraKeyframe& key) { return value < key.frame; });
		const auto& next = *upper;
		const auto& previous = *(upper - 1);
		const float t = static_cast<float>(frame - previous.frame) /
			static_cast<float>(next.frame - previous.frame);

		CameraPose result{};
		XMStoreFloat3(&result.position, XMVectorLerp(
			XMLoadFloat3(&previous.pose.position), XMLoadFloat3(&next.pose.position), t));
		XMStoreFloat4(&result.rotation, XMQuaternionSlerp(
			XMLoadFloat4(&previous.pose.rotation), XMLoadFloat4(&next.pose.rotation), t));
		return result;
	}

	uint64_t CameraPath::end_frame() const {
		return keyframes_.empty() ? 0 : keyframes_.back().frame;
	}
}
