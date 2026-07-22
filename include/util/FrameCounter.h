#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include <ostream>

#include "ProgramArgument.h"

namespace util {

	class FrameCounter {

	public:
		FrameCounter() = default;

        struct CountedData {
            double time_min_ms = 0.0;
            double time_median_ms = 0.0;
            double time_max_ms = 0.0;
            double time_avg_ms = 0.0;
            double time_p01_ms = 0.0;
            double time_p10_ms = 0.0;
            double time_p90_ms = 0.0;
            double time_p99_ms = 0.0;

            static std::string get_header_string() {
                return std::string(
                    "time_min_ms,time_median_ms,time_max_ms,time_avg_ms,"
                    "time_p01_ms,time_p10_ms,time_p90_ms,time_p99_ms");
            }

            std::string to_string() const {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(5)
                    << time_min_ms << ','
                    << time_median_ms << ','
                    << time_max_ms << ','
                    << time_avg_ms << ','
                    << time_p01_ms << ','
                    << time_p10_ms << ','
                    << time_p90_ms << ','
                    << time_p99_ms;
                return oss.str();
            }

            friend std::ostream& operator<<(std::ostream& os, const CountedData& data) {
                return os << data.to_string();
            }
        };

        struct WindowedData {
            uint64_t frame = 0;
            std::vector<double> time_avg_ms;
            bool has_index_count = false;
            double index_count = 0.0;
        };

        void init(const util::ProgramArgument& arg);
		void tick(
			std::vector<double>& measure,
			bool to_record_index_count = false,
			double index_count = 0.0);
        bool to_terminate() const;
		std::vector<CountedData> summarize();
		std::vector<WindowedData> summarize_windows(uint32_t window_frames) const;

	private:
		uint64_t frames_ = 0;
		uint64_t frame_to_start_measure_ = 0;
		uint64_t frame_to_end_measure_ = 0;
		uint64_t frame_to_terminate_ = 0;
		std::vector<std::vector<double>> frame_times_;
		std::vector<double> frame_index_counts_;
	};
}
