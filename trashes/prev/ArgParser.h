#pragma once

#include <vector>
#include <string>

#include "util/ProgramArgument.h"

class ArgParser {
public:
	static util::ProgramArgument parse(const std::vector<std::string>& args);
};

