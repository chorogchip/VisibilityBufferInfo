#include "util/FrameCounter.h"

#include <algorithm>
#include <vector>

#include "util/Logger.h"
#include "util/Constants.h"

namespace util {

	void FrameCounter::init(const util::ProgramArgument& arg) {
		const int dimension = util::Constants::TIMER_SLOT_COUNT;
		frame_times_ = std::vector<std::vector<double>>(dimension, std::vector<double>());
		frames_ = 0;
		frame_to_start_measure_ = arg.warmup_frames;
		frame_to_end_measure_ = frame_to_start_measure_ + arg.measure_frames;
		frame_to_terminate_ = frame_to_end_measure_ + 60;
		frame_index_counts_.clear();
	}

	void FrameCounter::tick(
		std::vector<double>& measures,
		bool to_record_index_count,
		double index_count) {
		if (frame_to_start_measure_ <= frames_ && frames_ < frame_to_end_measure_) {
			for (int i = 0; i < static_cast<int>(measures.size()); ++i) {
				frame_times_[i].push_back(measures[i]);
			}

			if (to_record_index_count)
				frame_index_counts_.push_back(index_count);
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

	std::vector<FrameCounter::WindowedData> FrameCounter::summarize_windows(
		uint32_t window_frames) const {
		Logger::g_logger.assert_with_log(window_frames > 0, "profile window must be greater than 0");
		std::vector<WindowedData> results;
		const size_t sample_count = frame_times_.empty() ? 0 : frame_times_.front().size();

		util::Logger::g_logger << "[FrameCounter] "
			<< "frames{" << frames_
			<< "} measure_start{" << frame_to_start_measure_
			<< "} measure_end{" << frame_to_end_measure_
			<< "} sample_count{" << sample_count
			<< "} window_frames{" << window_frames
			<< "}\n";

		for (size_t begin = 0; begin < sample_count; begin += window_frames) {
			const size_t end = (std::min)(begin + window_frames, sample_count);
			WindowedData result{};
			result.frame = begin;
			result.time_avg_ms.resize(frame_times_.size());
			for (size_t pass = 0; pass < frame_times_.size(); ++pass) {
				double sum = 0.0;
				double val_min = std::numeric_limits<double>::max() * 0.5;
				double val_max = 0.0;
				for (size_t frame = begin; frame < end; ++frame) {
					double x = frame_times_[pass][frame];
					sum += x;
					val_min = std::min(val_min, x);
					val_max = std::max(val_max, x);
				}
				sum -= (val_min + val_max);
				if (end - begin > 2)
					result.time_avg_ms[pass] = sum / static_cast<double>(end - begin - 2);
			}

			if (frame_index_counts_.size() == sample_count) {
				double sum = 0.0;
				for (size_t frame = begin; frame < end; ++frame)
					sum += frame_index_counts_[frame];
				result.has_index_count = true;
				result.index_count = sum / static_cast<double>(end - begin);
			}

			results.push_back(std::move(result));
		}
		return results;
	}
	
	bool FrameCounter::to_terminate() const {
		return frame_to_terminate_ <= frames_;
	}
}
