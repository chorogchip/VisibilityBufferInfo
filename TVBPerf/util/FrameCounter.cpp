#include "util/FrameCounter.h"

#include <algorithm>

namespace util {

	void FrameCounter::init(uint64_t frame_to_start_measure, uint64_t frame_to_end_measure, uint64_t frame_to_terminate) {
		frames_ = 0;
		frame_to_start_measure_ = frame_to_start_measure;
		frame_to_end_measure_ = frame_to_end_measure;
		frame_to_terminate_ = frame_to_terminate;
	}

	void FrameCounter::tick(float frame_time) {
		if (frame_to_start_measure_ <= frames_ && frames_ < frame_to_end_measure_) {
			frame_times_.push_back(frame_time);
		}
		frames_++;
	}

	FrameCounter::CountedData FrameCounter::summarize() {
		FrameCounter::CountedData ret{};
		
		const size_t sz = frame_times_.size();
		if (sz == 0) return ret;

		std::sort(frame_times_.begin(), frame_times_.end());
		ret.frame_time_min = frame_times_.front();
		ret.frame_time_mid = frame_times_[sz / 2];
		ret.frame_time_max = frame_times_.back();

		ret.frame_time_p01 = frame_times_[static_cast<size_t>(static_cast<float>(sz) * 0.01f)];
		ret.frame_time_p10 = frame_times_[static_cast<size_t>(static_cast<float>(sz) * 0.10f)];
		ret.frame_time_p90 = frame_times_[static_cast<size_t>(static_cast<float>(sz) * 0.90f)];
		ret.frame_time_p99 = frame_times_[static_cast<size_t>(static_cast<float>(sz) * 0.99f)];

		double sum = 0.0;
		for (auto f : frame_times_) sum += f;
		ret.frame_time_avg = static_cast<float>(sum / static_cast<double>(sz));

		ret.is_valid = true;
		return ret;
	}
	
	bool FrameCounter::to_terminate() const {
		return frame_to_terminate_ <= frames_;
	}
}