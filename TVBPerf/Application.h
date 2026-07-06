#pragma once

#include <Windows.h>

#include "util/ProgramArgument.h"
#include "render/RendererBase.h"

class Application {
public:
	~Application();
	void run(HINSTANCE h_instance, int n_show_cmd);

private:
	ProgramArgument program_argument_;
	void parse_args();
};