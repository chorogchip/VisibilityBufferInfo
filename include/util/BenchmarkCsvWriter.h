#pragma once

#include <filesystem>

#include "ProgramArgument.h"
#include "util/FrameCounter.h"

namespace util {

    std::filesystem::path get_scene_fingerprint_output_path(
        const std::string& output_filepath);

    void write_benchmark_csv(
        const std::filesystem::path& path,
        const ProgramArgument& arguments,
        const ProgramResult& result);

    void write_windowed_benchmark_csv(
        const std::filesystem::path& path,
        const std::array<std::string, util::Constants::TIMER_SLOT_COUNT>& pass_names,
        const std::vector<FrameCounter::WindowedData>& windows);
}
