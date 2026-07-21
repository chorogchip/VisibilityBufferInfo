#pragma once

#include "ProgramArgument.h"

namespace util {
	class ProgramArgumentValidator {

	public:
		static void validate_program_args(const ProgramArgument& arg);
	};
}