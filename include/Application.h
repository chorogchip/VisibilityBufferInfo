#pragma once

#include <Windows.h>
#include <memory>

#include "util/ProgramArgument.h"
#include "render/RendererBase.h"
#include "Win32Window.h"

class Application {
public:
	~Application();
	void run(HINSTANCE h_instance, int n_show_cmd);

private:
	util::ProgramArgument program_argument_;
    Win32Window window_;
    std::unique_ptr<RendererBase> renderer_;

	void parse_args();
    bool handle_key_down(WPARAM key);
};
