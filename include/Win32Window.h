#pragma once

#include <Windows.h>
#include <cstdint>
#include <functional>

class Win32Window {
public:
    void create(HINSTANCE h_instance, int n_show_cmd, uint32_t width, uint32_t height, const wchar_t* title);
    bool process_messages();

    HWND hwnd() const { return hwnd_; }
    void set_key_down_handler(std::function<bool(WPARAM)> handler);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle_message(UINT msg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    std::function<bool(WPARAM)> key_down_handler_;
};
