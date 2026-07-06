#pragma once

#include <vector>
#include <string>

#include "util/ProgramArgument.h"

class ArgParser {
public:
	static ProgramArgument parse(const std::vector<std::string>& args);
};

