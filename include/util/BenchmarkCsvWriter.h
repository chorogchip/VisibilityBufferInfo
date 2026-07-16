#pragma once

#include <filesystem>

#include "ProgramArgument.h"

namespace util {

    std::filesystem::path get_scene_fingerprint_output_path(
        const std::string& output_filepath);

    void write_benchmark_csv(
        const std::filesystem::path& path,
        const ProgramArgument& arguments,
        const ProgramResult& result);
}
