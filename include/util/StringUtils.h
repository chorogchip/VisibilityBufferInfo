#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace util {
    template <typename>
    inline constexpr bool always_false_v = false;

    template <typename T>
    T parse_value(std::string_view s) {
        if constexpr (std::is_same_v<T, std::string>) {
            return std::string{ s };
        } else if constexpr (std::is_same_v<T, bool>) {
            if (s == "true" || s == "1") return true;
            if (s == "false" || s == "0") return false;

            throw std::runtime_error{
                "parse bool error: " + std::string{ s }
            };
        } else if constexpr (std::is_arithmetic_v<T>) {
            T value{};

            const char* begin = s.data();
            const char* end = begin + s.size();

            const auto [ptr, ec] = std::from_chars(begin, end, value);

            if (ec != std::errc{} || ptr != end) {
                throw std::runtime_error{
                    "parse value error: " + std::string{ s }
                };
            }

            return value;
        } else {
            static_assert(always_false_v<T>, "Unsupported argument type");
        }
    }
}