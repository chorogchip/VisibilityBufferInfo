#pragma once

#include <cstdint>
#include <vector>

namespace util {

	class FrameCounter {

	public:
		FrameCounter() = default;

		struct CountedData {
			bool is_valid;
			float frame_time_min;
			float frame_time_mid;
			float frame_time_max;
			float frame_time_avg;
			float frame_time_p01;
			float frame_time_p10;
			float frame_time_p90;
			float frame_time_p99;
		};

		void init(uint64_t frame_to_start_measure, uint64_t frame_to_end_measure, uint64_t frame_to_terminate);
		void tick(float frame_time);
		CountedData summarize();
		bool to_terminate() const;

	private:
		uint64_t frames_ = 0;
		uint64_t frame_to_start_measure_ = 0;
		uint64_t frame_to_end_measure_ = 0;
		uint64_t frame_to_terminate_ = 0;
		std::vector<float> frame_times_;
	};
}