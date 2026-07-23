#pragma once

#include <source_location>
#include <sstream>
#include <iostream>
#include <mutex>

namespace util {
	class Logger {

	private:
		std::ostringstream logging_stream_;
		std::mutex mutex_;

	public:
		Logger() = default;
		Logger(const Logger&) = delete;
		Logger& operator=(const Logger&) = delete;

		void flush();

		void add_logging_info(const char* log_info);

		void assert_with_log(
			bool expression,
			const std::source_location& loc = std::source_location::current());

		void assert_with_log(
			bool expression,
			const char* log_info,
			const std::source_location& loc = std::source_location::current());

		void assert_with_log_add_overflow(
			size_t a,
			size_t b,
			size_t limit,
			const char* log_info,
			const std::source_location& loc = std::source_location::current());

		void assert_with_log_mul_overflow(
			size_t a,
			size_t b,
			size_t limit,
			const char* log_info,
			const std::source_location& loc = std::source_location::current());

		template<typename T>
		Logger& operator<<(const T& data) {
			std::lock_guard<std::mutex> lock(mutex_);
			logging_stream_ << data;
			return *this;
		}

		Logger& operator<<(std::ostream& (*manip)(std::ostream&)) {
			std::lock_guard<std::mutex> lock(mutex_);
			manip(logging_stream_);
			return *this;
		}

		static Logger g_logger;
	};
}