#pragma once

#include <Windows.h>
#include <string>
#include <source_location>

#include "util/Logger.h"

namespace util {

    class Utils {
    public:
        static void throw_if_failed(
            HRESULT hr,
            const char* message,
            const std::source_location& loc = std::source_location::current()) {

            if (SUCCEEDED(hr)) return;
            util::Logger::g_logger.assert_with_log(false, message, loc);
        }

        static void throw_win32_lasterr(const char* message) {
            Utils::throw_if_failed(HRESULT_FROM_WIN32(GetLastError()), message);
        }

        template<typename T>
        static constexpr T GetAlignedAddress(T address, T alignment) {
            return (address + (alignment - 1)) & ~(alignment - 1);
        }

        static std::string wstring_to_string(const std::wstring& ws) {
            if (ws.empty()) return std::string{};

            int len = WideCharToMultiByte(
                CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                nullptr, 0, nullptr, nullptr
            );

            std::string s(len, '\0');

            WideCharToMultiByte(
                CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()),
                &s[0], len, nullptr, nullptr);

            return s;
        }
        static void throw_if_failed(HRESULT hr) {
            throw_if_failed(hr, "no cause specified");
        }

    };

}