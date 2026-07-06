#pragma once

#include <Windows.h>

#include "util/ProgramArgument.h"
#include "render/RendererBase.h"

class Application {
public:
	void run(HINSTANCE h_instance, int n_show_cmd);

private:
	ProgramArgument program_argument_;
	std::unique_ptr<RendererBase> renderer_;

	void parse_args();
};