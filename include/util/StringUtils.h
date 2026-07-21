#pragma once

#include <charconv>
#include <string>
#include <string_view>
#include <type_traits>

#include "util/Logger.h"

namespace util {

    class StringUtils {
    public:
        static bool equals_ignore_case(std::wstring_view lhs, std::wstring_view rhs);

        template <typename T>
        static T parse_value(std::string_view s) {
            if constexpr (std::is_same_v<T, std::string>) {
                return std::string{ s };
            }
            else if constexpr (std::is_same_v<T, bool>) {
                if (s == "true" || s == "1") return true;
                if (s == "false" || s == "0") return false;

                Logger::g_logger << "parse bool error: " << s << '\n';
                Logger::g_logger.assert_with_log(false, "failed to parse bool value");
            }
            else if constexpr (std::is_arithmetic_v<T>) {
                T value{};
                const char* begin = s.data();
                const char* end = begin + s.size();

                const auto [ptr, ec] = std::from_chars(begin, end, value);
                if (ec != std::errc{} || ptr != end) {
                    Logger::g_logger << "parse value error: " << s << '\n';
                    Logger::g_logger.assert_with_log(false, "failed to parse arithmetic value");
                }

                return value;
            }
            else {
                static_assert(!std::is_same_v<T, T>, "Unsupported argument type");
            }
            return T{};
        }
    };

}
