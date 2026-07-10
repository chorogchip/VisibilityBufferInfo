#pragma once

#include <Windows.h>
#include <stdexcept>
#include <string>
#include <source_location>

#include "util/Logger.h"

class Utils {
public:
    // if debug, then throw runtime error
    // if release, log by txt
    [[noreturn]] static void panic(const char* message, const std::source_location& loc = std::source_location::current()) {

        const char* safe_message = message ? message : "Unknown panic";

#ifdef _DEBUG
        throw std::runtime_error(std::string(safe_message));
#else
        util::Logger::g_logger.assert_with_log(false, safe_message, loc);
        std::terminate();
#endif
    }

    static void throw_if_failed(HRESULT hr, const char* message, const std::source_location& loc = std::source_location::current()) {
        if (FAILED(hr)) {
            panic((std::string("HRESULT failed on ") + message).c_str(), loc);
        }
    }

    static void throw_win32_lasterr(const char* message) {
        throw_if_failed(HRESULT_FROM_WIN32(GetLastError()), message);
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
            &s[0], len, nullptr, nullptr );

        return s;
    }

    static void throw_if_failed(HRESULT hr) {
        throw_if_failed(hr, "no cause specified");
    }

};