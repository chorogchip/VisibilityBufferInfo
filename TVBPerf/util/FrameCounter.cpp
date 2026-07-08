#include "util/FrameCounter.h"

#include <algorithm>

namespace util {

	void FrameCounter::init(uint64_t frame_to_start_measure, uint64_t frame_to_end_measure, uint64_t frame_to_terminate) {
		frames_ = 0;
		frame_to_start_measure_ = frame_to_start_measure;
		frame_to_end_measure_ = frame_to_end_measure;
		frame_to_terminate_ = frame_to_terminate;
	}

	void FrameCounter::tick(double pass0, double pass1) {
		if (frame_to_start_measure_ <= frames_ && frames_ < frame_to_end_measure_) {
			frame_times_[0].push_back(pass0);
			frame_times_[1].push_back(pass1);
			frame_times_[2].push_back(pass0 + pass1);
		}
		frames_++;
	}

	std::array<FrameCounter::CountedData, 3> FrameCounter::summarize() {
		std::array<FrameCounter::CountedData, 3> rets{};

		for (int i = 0; i < 3; ++i) {
			auto& ret = rets[i];

			auto& frame_times = frame_times_[i];

			const size_t sz = frame_times.size();
			if (sz == 0) continue;

			std::sort(frame_times_.begin(), frame_times_.end());
			ret.frame_time_min = frame_times.front();
			ret.frame_time_mid = frame_times[sz / 2];
			ret.frame_time_max = frame_times.back();

			ret.frame_time_p01 = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.01f)];
			ret.frame_time_p10 = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.10f)];
			ret.frame_time_p90 = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.90f)];
			ret.frame_time_p99 = frame_times[static_cast<size_t>(static_cast<float>(sz) * 0.99f)];

			double sum = 0.0;
			for (auto f : frame_times) sum += f;
			ret.frame_time_avg = static_cast<float>(sum / static_cast<double>(sz));
		}
		return rets;
	}
	
	bool FrameCounter::to_terminate() const {
		return frame_to_terminate_ <= frames_;
	}
}