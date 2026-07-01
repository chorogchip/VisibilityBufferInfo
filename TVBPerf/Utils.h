#pragma once

#include <Windows.h>
#include <stdexcept>

class Utils {
public:
    static void throw_if_failed(HRESULT hr) {
        if (FAILED(hr)) {
            throw std::runtime_error("HRESULT failed");
        }
    }

    template<typename T>
    static constexpr T GetAlignedAddress(T address, T alignment) {
        return (address + (alignment - 1)) & ~(alignment - 1);
    }

};