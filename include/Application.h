#pragma once

#include <Windows.h>
#include <memory>

#include "ProgramArgument.h"
#include "Win32Window.h"
#include "render/renderer/RendererBase.h"

class Application {
public:
	~Application();
	void run(HINSTANCE h_instance, int n_show_cmd);

private:
	util::ProgramArgument program_argument_;
    Win32Window window_;
    std::unique_ptr<RendererBase> renderer_;

	void parse_args();
    void update_camera_input(float delta_seconds);
};
