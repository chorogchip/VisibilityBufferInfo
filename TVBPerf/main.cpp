#include <windows.h>
#include <stdexcept>
#include "RendererForward.h"

static RendererForward g_renderer;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            PostQuitMessage(0);
            return 0;
        }
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain( _In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {

    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(nShowCmd);

    try {
        const uint32_t width = 1280;
        const uint32_t height = 720;
         
        WNDCLASSEX wc{};
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"TVBPerf";

        RegisterClassEx(&wc);

        RECT window_rect{};
        window_rect.left = 0;
        window_rect.top = 0;
        window_rect.right = width;
        window_rect.bottom = height;

        AdjustWindowRect(
            &window_rect,
            WS_OVERLAPPEDWINDOW,
            FALSE);

        HWND hwnd = CreateWindowEx(
            0,
            wc.lpszClassName,
            L"Triangle Visibility Buffer Performance",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            window_rect.right - window_rect.left,
            window_rect.bottom - window_rect.top,
            nullptr,
            nullptr,
            hInstance,
            nullptr);

        if (!hwnd) {
            throw std::runtime_error("CreateWindowEx failed");
        }

        ShowWindow(hwnd, nShowCmd);
        UpdateWindow(hwnd);

        g_renderer.init(hwnd, width, height);

        MSG msg{};

        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                g_renderer.render();
            }
        }

    } catch (const std::exception&) {
        MessageBoxA(nullptr, "Fatal error", "Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    return 0;
}