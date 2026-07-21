#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include <utility>

class Win32Window {
public:
    void create(
        HINSTANCE h_instance,
        int n_show_cmd,
        uint32_t width,
        uint32_t height,
        const wchar_t* title);

    bool process_messages();

    HWND hwnd() const { return hwnd_; }
    bool is_key_down(uint32_t key) const;
    std::pair<int, int> consume_mouse_delta();

private:

    static LRESULT CALLBACK WndProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam);

    LRESULT handle_message(
        UINT msg,
        WPARAM wParam,
        LPARAM lParam);

    HWND hwnd_ = nullptr;
    std::array<bool, 256> key_states_{};

    bool rotating_camera_ = false;
    int mouse_x_ = 0;
    int mouse_y_ = 0;
    int mouse_delta_x_ = 0;
    int mouse_delta_y_ = 0;
};
