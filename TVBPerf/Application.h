#pragma once

#include <Windows.h>

#include "util/ProgramArgument.h"

class Application {
public:
	void parse_args();
	void init(HINSTANCE h_instance, int n_show_cmd);
	void run();
	void close();

private:
	ProgramArgument program_argument;
};

