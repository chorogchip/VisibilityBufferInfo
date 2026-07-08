#include "util/FrameCounter.h"

#include <algorithm>
#include <vector>

namespace util {

	void FrameCounter::init(int dimension, uint64_t frame_to_start_measure, uint64_t frame_to_end_measure, uint64_t frame_to_terminate) {
		frame_times_ = std::vector<std::vector<double>>(dimension, std::vector<double>());
		frames_ = 0;
		frame_to_start_measure_ = frame_to_start_measure;
		frame_to_end_measure_ = frame_to_end_measure;
		frame_to_terminate_ = frame_to_terminate;
	}

	void FrameCounter::tick(std::vector<double>& measures) {
		if (frame_to_start_measure_ <= frames_ && frames_ < frame_to_end_measure_) {
			for (int i = 0; i < static_cast<int>(measures.size()); ++i) {
				frame_times_[i].push_back(measures[i]);
			}
		}
		frames_++;
	}

	std::vector<FrameCounter::CountedData> FrameCounter::summarize() {
		std::vector<FrameCounter::CountedData> rets{};

		for (int i = 0; i < static_cast<int>(frame_times_.size()); ++i) {
			rets.emplace_back();
			auto& ret = rets.back();

			auto& frame_times = frame_times_[i];

			const size_t sz = frame_times.size();
			if (sz == 0) continue;

			std::sort(frame_times.begin(), frame_times.end());
			ret.time_min_ms = frame_times.front();
			ret.time_median_ms = frame_times[sz / 2];
			ret.time_max_ms = frame_times.back();

			ret.time_p01_ms = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.01f)];
			ret.time_p10_ms = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.10f)];
			ret.time_p90_ms = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.90f)];
			ret.time_p99_ms = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.99f)];

			double sum = 0.0;
			for (auto f : frame_times) sum += f;
			ret.time_avg_ms = static_cast<float>(sum / static_cast<double>(sz));
		}
		return rets;
	}
	
	bool FrameCounter::to_terminate() const {
		return frame_to_terminate_ <= frames_;
	}
}
