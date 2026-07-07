#include "Application.h"

#include <Windows.h>
#include <stdexcept>
#include <string>
#include <memory>

#include "util/Utils.h"
#include "util/ArgParser.h"
#include "render/RendererForward.h"
#include "render/RendererForwardPrepass.h"
#include "render/RendererDeferred.h"

static std::unique_ptr<RendererBase> renderer_;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

Application::~Application() {
    renderer_ = nullptr;
}

void Application::parse_args() {
    std::vector<std::string> args;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (int i = 1; i < argc; ++i)
        args.push_back(Utils::wstring_to_string(argv[i]));

    program_argument_ = ArgParser::parse(args);
}

void Application::run(HINSTANCE h_instance, int n_show_cmd) {
    parse_args();

    const uint32_t width = program_argument_.window_width;
    const uint32_t height = program_argument_.window_height;

    WNDCLASSEX wc{};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = h_instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"TVBPerf";

    RegisterClassEx(&wc);

    RECT window_rect{};
    window_rect.left = 0;
    window_rect.top = 0;
    window_rect.right = width;
    window_rect.bottom = height;

    AdjustWindowRect(&window_rect, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0, wc.lpszClassName, L"Triangle Visibility Buffer Performance",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        window_rect.right - window_rect.left,
        window_rect.bottom - window_rect.top,
        nullptr, nullptr, h_instance, nullptr);

    if (!hwnd) throw std::runtime_error("CreateWindowEx failed");

    ShowWindow(hwnd, n_show_cmd);
    UpdateWindow(hwnd);

    switch (program_argument_.renderer_variant) {
    case 0: default:
        renderer_ = std::unique_ptr<RendererBase>(new rndr::RendererForward{});
        break;
    case 1:
        renderer_ = std::unique_ptr<RendererBase>(new rndr::RendererForwardPrePass{});
        break;
    case 2:
        renderer_ = std::unique_ptr<RendererBase>(new rndr::RendererDeferred{});
    }
    
    renderer_->init(hwnd, program_argument_);

    MSG msg{};

    while (msg.message != WM_QUIT) {
        if (renderer_->to_terminate()) {
            PostQuitMessage(0);
            break;
        } else if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            renderer_->render();
        }
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }

        if (!renderer_) {
            break;
        }

        constexpr float move_speed = 0.3f;
        constexpr float turn_speed = 3.0f;

        switch (wParam) {
        case 'W':
            renderer_->camera_.move_forward(move_speed);
            return 0;

        case 'S':
            renderer_->camera_.move_forward(-move_speed);
            return 0;

        case 'A':
            renderer_->camera_.move_right(-move_speed);
            return 0;

        case 'D':
            renderer_->camera_.move_right(move_speed);
            return 0;

        case 'Q':
            renderer_->camera_.turn(-turn_speed);
            return 0;

        case 'E':
            renderer_->camera_.turn(turn_speed);
            return 0;

        case VK_SPACE:
            renderer_->camera_.move_pos(0.0f, move_speed, 0.0f);
            return 0;

        case VK_SHIFT:
            renderer_->camera_.move_pos(0.0f, -move_speed, 0.0f);
            return 0;
        }

        break;
    }

    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}