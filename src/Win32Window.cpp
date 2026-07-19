#include "Win32Window.h"

#include <utility>

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

void Win32Window::set_key_down_handler(std::function<bool(WPARAM)> handler) {
    key_down_handler_ = std::move(handler);
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

        if (key_down_handler_ && key_down_handler_(wParam)) {
            return 0;
        }

        break;
    }

    return DefWindowProc(hwnd_, msg, wParam, lParam);
}
