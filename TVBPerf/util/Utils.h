#pragma once

#include <Windows.h>
#include <stdexcept>
#include <string>

class Utils {
public:
    static void throw_if_failed(HRESULT hr, const char* message) {
        if (FAILED(hr)) {
            throw std::runtime_error(std::string("HRESULT failed on ") + message);
        }
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