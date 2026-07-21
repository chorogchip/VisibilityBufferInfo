#include "Application.h"

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "util/Utils.h"
#include "util/ProgramArgumentValidator.h"
#include "render/renderer/RendererFactory.h"
#include "engine/TextureLoader.h"

Application::~Application() {
    renderer_ = nullptr;
    eng::TextureLoader::close();
}

void Application::parse_args() {
    std::vector<std::string> args;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    for (int i = 1; i < argc; ++i)
        args.push_back(Utils::wstring_to_string(argv[i]));

    LocalFree(argv);

    program_argument_ = util::ProgramArgument::from_args(args);
    util::ProgramArgumentValidator::validate_program_args(program_argument_);
}

void Application::run(HINSTANCE h_instance, int n_show_cmd) {

    eng::TextureLoader::init();

    this->parse_args();

    window_.create(h_instance, n_show_cmd,
        program_argument_.window_width, program_argument_.window_height,
        L"Visibility Buffer Performance");

    renderer_ = rndr::create_renderer(program_argument_.renderer_variant);
    renderer_->init(window_.hwnd(), program_argument_);

    bool running = true;
    auto previous_time = std::chrono::steady_clock::now();
    while (running) {
        if (program_argument_.auto_terminate && renderer_->to_terminate()) {
            PostQuitMessage(0);
            break;
        }

        running = window_.process_messages();
        if (running) {
            const auto current_time = std::chrono::steady_clock::now();
            const float delta_seconds = (std::min)(
                std::chrono::duration<float>(current_time - previous_time).count(), 0.1f);
            previous_time = current_time;
            update_camera_input(delta_seconds);
            renderer_->render();
        }
    }

    renderer_->close();
    renderer_ = nullptr;
}

void Application::update_camera_input(float delta_seconds) {

    const auto [mouse_x, mouse_y] = window_.consume_mouse_delta();

    if (!renderer_ || program_argument_.camera_mode == 2) return;

    constexpr float MOVE_SPEED = 6.0f;
    constexpr float MOUSE_SENSITIVITY = 0.003f;
    const float distance = MOVE_SPEED * delta_seconds;

    if (window_.is_key_down('W')) renderer_->camera_.move_forward(distance);
    if (window_.is_key_down('S')) renderer_->camera_.move_forward(-distance);
    if (window_.is_key_down('A')) renderer_->camera_.move_right(-distance);
    if (window_.is_key_down('D')) renderer_->camera_.move_right(distance);
    if (window_.is_key_down(VK_SPACE)) renderer_->camera_.move_pos(0.0f, distance, 0.0f);
    if (window_.is_key_down(VK_SHIFT)) renderer_->camera_.move_pos(0.0f, -distance, 0.0f);

    renderer_->camera_.turn_right(static_cast<float>(mouse_x) * MOUSE_SENSITIVITY);
    renderer_->camera_.turn_up(static_cast<float>(mouse_y) * MOUSE_SENSITIVITY);
}