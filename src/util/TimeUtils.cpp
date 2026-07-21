#include "util/TimeUtils.h"

#include <chrono>
#include <sstream>

namespace util {

    std::string TimeUtils::make_current_time_string() {
        const auto now = std::chrono::system_clock::now();
        const std::time_t current_time = std::chrono::system_clock::to_time_t(now);
        std::tm local_time{};
        localtime_s(&local_time, &current_time);

        std::ostringstream stream;
        stream << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S");
        return stream.str();
    }
}