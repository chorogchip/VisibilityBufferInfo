#include "Win32Window.h"

#include <windowsx.h>

#include "util/Logger.h"

void Win32Window::create(
    HINSTANCE h_instance,
    int n_show_cmd,
    uint32_t width,
    uint32_t height,
    const wchar_t* title) {

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32Window::WndProc;
    wc.hInstance = h_instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"TVBPerf";

    RegisterClassEx(&wc);

    RECT window_rect{};
    window_rect.left = 0;
    window_rect.top = 0;
    window_rect.right = static_cast<LONG>(width);
    window_rect.bottom = static_cast<LONG>(height);

    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowEx(
        0, wc.lpszClassName, title,
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr, nullptr, h_instance, this);

    util::Logger::g_logger.assert_with_log(hwnd_ != nullptr, "CreateWindowEx failed");

    ShowWindow(hwnd_, n_show_cmd);
    UpdateWindow(hwnd_);
}

bool Win32Window::process_messages() {
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return true;
}

bool Win32Window::is_key_down(uint32_t key) const {
    return key < key_states_.size() && key_states_[key];
}

std::pair<int, int> Win32Window::consume_mouse_delta() {
    const std::pair<int, int> delta{ mouse_delta_x_, mouse_delta_y_ };
    mouse_delta_x_ = 0;
    mouse_delta_y_ = 0;
    return delta;
}

LRESULT CALLBACK Win32Window::WndProc
(HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam) {

    Win32Window* window = nullptr;

    if (msg == WM_NCCREATE) {
        auto* create_struct = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<Win32Window*>(create_struct->lpCreateParams);
        window->hwnd_ = hwnd;
        SetWindowLongPtr(
            hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Win32Window*>(
            GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->handle_message(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT Win32Window::handle_message(
    UINT msg,
    WPARAM wParam,
    LPARAM lParam) {

    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }

        if (wParam < key_states_.size()) key_states_[wParam] = true;
        return 0;

    case WM_KEYUP:
        if (wParam < key_states_.size()) key_states_[wParam] = false;
        return 0;

    case WM_RBUTTONDOWN:
        rotating_camera_ = true;
        mouse_x_ = GET_X_LPARAM(lParam);
        mouse_y_ = GET_Y_LPARAM(lParam);
        SetCapture(hwnd_);
        return 0;

    case WM_RBUTTONUP:
        rotating_camera_ = false;
        ReleaseCapture();
        return 0;

    case WM_MOUSEMOVE:
        if (rotating_camera_) {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            mouse_delta_x_ += x - mouse_x_;
            mouse_delta_y_ += y - mouse_y_;
            mouse_x_ = x;
            mouse_y_ = y;
        }
        return 0;

    case WM_KILLFOCUS:
        key_states_.fill(false);
        rotating_camera_ = false;
        mouse_delta_x_ = 0;
        mouse_delta_y_ = 0;
        if (GetCapture() == hwnd_) ReleaseCapture();
        return 0;
    }

    return DefWindowProc(hwnd_, msg, wParam, lParam);
}
