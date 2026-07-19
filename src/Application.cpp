#include "Application.h"

#include <Windows.h>
#include <string>
#include <vector>

#include "util/Utils.h"
#include "render/renderer/RendererFactory.h"

Application::~Application() {
    renderer_ = nullptr;
}

void Application::parse_args() {
    std::vector<std::string> args;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (int i = 1; i < argc; ++i)
        args.push_back(Utils::wstring_to_string(argv[i]));

    LocalFree(argv);

    program_argument_ = util::ProgramArgument::from_args(args);
}

void Application::run(HINSTANCE h_instance, int n_show_cmd) {
    this->parse_args();

    window_.create(h_instance, n_show_cmd,
        program_argument_.window_width, program_argument_.window_height,
        L"Visibility Buffer Performance");

    renderer_ = rndr::create_renderer(program_argument_.renderer_variant);
    window_.set_key_down_handler([this](WPARAM key) { return handle_key_down(key); });
    renderer_->init(window_.hwnd(), program_argument_);

    bool running = true;
    while (running) {
        if (program_argument_.auto_terminate && renderer_->to_terminate()) {
            PostQuitMessage(0);
            break;
        }

        running = window_.process_messages();
        if (running) renderer_->render();
    }

    renderer_->close();
    renderer_ = nullptr;
}

bool Application::handle_key_down(WPARAM key) {
    if (!renderer_) return false;

    constexpr float move_speed = 0.3f;
    constexpr float turn_speed = 3.0f * 0.01f;

    switch (key) {
    case 'W':
        renderer_->camera_.move_forward(move_speed);
        return true;
    case 'S':
        renderer_->camera_.move_forward(-move_speed);
        return true;
    case 'A':
        renderer_->camera_.move_right(-move_speed);
        return true;
    case 'D':
        renderer_->camera_.move_right(move_speed);
        return true;
    case 'Q':
        renderer_->camera_.turn_right(-turn_speed);
        return true;
    case 'E':
        renderer_->camera_.turn_right(turn_speed);
        return true;
    case 'R':
        renderer_->camera_.turn_up(turn_speed);
        return true;
    case 'F':
        renderer_->camera_.turn_up(-turn_speed);
        return true;
    case VK_SPACE:
        renderer_->camera_.move_pos(0.0f, move_speed, 0.0f);
        return true;
    case VK_SHIFT:
        renderer_->camera_.move_pos(0.0f, -move_speed, 0.0f);
        return true;
    }

    return false;
}
