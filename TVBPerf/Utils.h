#pragma once

#include <Windows.h>
#include <stdexcept>

class Utils {
public:
    static void throw_if_failed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw std::runtime_error("HRESULT failed");
        }
    }

};