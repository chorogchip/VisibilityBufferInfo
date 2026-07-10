#include "util/FrameCounter.h"

namespace util {

	void FrameCounter::init(uint64_t start_measure_frame_offset, uint64_t measure_frame_count, uint64_t terminate_frame_count) {
		frames_ = 0;
		frame_to_start_measure_ = start_measure_frame_offset;
		frame_to_end_measure_ = start_measure_frame_offset + measure_frame_count;
		frame_to_terminate_ = start_measure_frame_offset + measure_frame_count + terminate_frame_count;
	}

	void FrameCounter::record(uint32_t index, double value) {
		frame_times_.push_back(std::make_tuple(frames_, index, value));
	}

	void FrameCounter::next_tick() {
		frames_++;
	}

	bool FrameCounter::to_terminate() const {
		return frame_to_terminate_ <= frames_;
	}
}
