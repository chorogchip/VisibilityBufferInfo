#pragma once

#include <filesystem>

#include "util/ProgramArgument.h"

namespace util {

    void write_benchmark_csv(
        const std::filesystem::path& path,
        const ProgramArgument& arguments,
        const ProgramResult& result);

}
