#include "util/Logger.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace util {

    Logger Logger::g_logger;

    void Logger::flush() {
        try {
            std::string log_str = logging_stream_.str();

            const std::filesystem::path log_dir = std::filesystem::current_path() / "logs";
            std::filesystem::create_directories(log_dir);

            const auto now = std::chrono::system_clock::now();
            const std::time_t current_time = std::chrono::system_clock::to_time_t(now);

            std::tm local_time{};

#ifdef _WIN32
            localtime_s(&local_time, &current_time);
#else
            localtime_r(&current_time, &local_time);
#endif

            std::ostringstream filename_stream;

            filename_stream
                << "log_"
                << std::put_time(&local_time, "%Y-%m-%d_%H-%M-%S")
                << ".txt";

            const std::filesystem::path log_path = log_dir / filename_stream.str();
            std::ofstream output_file(log_path, std::ios::out | std::ios::binary);

            if (output_file.is_open()) {
                output_file << log_str;
                output_file.flush();

                std::cerr << "Log saved to: " << log_path.string() << '\n';
            } else {
                std::cerr << "the log file could not be opened.\n" << log_str;
            }
        } catch (const std::exception& exception) {
            std::cerr << "while writing the log: " << exception.what() << '\n';
        }
    }

    void Logger::add_logging_info(const char* log_info) {
        if (log_info == nullptr) return;

        std::lock_guard<std::mutex> lock(mutex_);
        logging_stream_ << log_info << '\n';
    }

    void Logger::assert_with_log(
        bool expression,
        const std::source_location& loc) {

        this->assert_with_log(expression, "", loc);
    }

    void Logger::assert_with_log(
        bool expression,
        const char* log_info,
        const std::source_location& loc) {

        if (expression) return;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            logging_stream_
                << "\n"
                << "========== ASSERT FAILED ==========\n"
                << "Message  : " << (log_info != nullptr ? log_info : "(null)") << '\n'
                << "File     : " << loc.file_name() << '\n'
                << "Line     : " << loc.line() << '\n'
                << "Column   : " << loc.column() << '\n'
                << "Function : " << loc.function_name() << '\n'
                << "===================================\n";
        }
        this->flush();
        std::abort();
    }

    void Logger::assert_with_log_add_overflow(
        size_t a,
        size_t b,
        size_t limit,
        const char* log_info,
        const std::source_location& loc) {

        const bool no_overflow =
            a <= limit &&
            b <= limit - a;
        this->assert_with_log(no_overflow, log_info, loc);
    }

    void Logger::assert_with_log_mul_overflow(
        size_t a,
        size_t b,
        size_t limit,
        const char* log_info,
        const std::source_location& loc) {

        const bool no_overflow =
            a == 0 ||
            b <= limit / a;
        assert_with_log(no_overflow, log_info, loc);
    }

}
