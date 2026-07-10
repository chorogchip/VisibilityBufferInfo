#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace util {

	class FrameCounter {

	public:
		FrameCounter() = default;

		void init(uint64_t frame_to_start_measure, uint64_t frame_to_end_measure, uint64_t frame_to_terminate);
		void record(uint32_t index, double value);
		void next_tick();
		bool to_terminate() const;

	private:
		uint64_t frames_ = 0;
		uint64_t frame_to_start_measure_ = 0;
		uint64_t frame_to_end_measure_ = 0;
		uint64_t frame_to_terminate_ = 0;
		std::vector<std::tuple<uint64_t, uint32_t, double>> frame_times_;
	};
}
