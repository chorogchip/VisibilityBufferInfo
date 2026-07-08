#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include <ostream>

namespace util {

	class FrameCounter {

	public:
		FrameCounter() = default;

        struct CountedData {
            double frame_time_min;
            double frame_time_mid;
            double frame_time_max;
            double frame_time_avg;
            double frame_time_p01;
            double frame_time_p10;
            double frame_time_p90;
            double frame_time_p99;

            std::string to_string_header() const {
                return std::string(
                    "frame_time_min,frame_time_mid,frame_time_max,frame_time_avg,"
                    "frame_time_p01,frame_time_p10,frame_time_p90,frame_time_p99\n");
            }
            std::string to_string() const {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(3)
                    << frame_time_min << ','
                    << frame_time_mid << ','
                    << frame_time_max << ','
                    << frame_time_avg << ','
                    << frame_time_p01 << ','
                    << frame_time_p10 << ','
                    << frame_time_p90 << ','
                    << frame_time_p99
                    << '\n';
                return oss.str();
            }

            friend std::ostream& operator<<(std::ostream& os, const CountedData& data) {
                return os << data.to_string();
            }
        };

		void init(uint64_t frame_to_start_measure, uint64_t frame_to_end_measure, uint64_t frame_to_terminate);
		void tick(double pass0, double pass1);
		std::array<CountedData, 3> summarize();
		bool to_terminate() const;

	private:
		uint64_t frames_ = 0;
		uint64_t frame_to_start_measure_ = 0;
		uint64_t frame_to_end_measure_ = 0;
		uint64_t frame_to_terminate_ = 0;
		std::array<std::vector<double>, 3> frame_times_;
	};
}