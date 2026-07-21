#include "util/BenchmarkCsvWriter.h"

#include <fstream>
#include <iomanip>
#include <iostream>

namespace util {

    std::filesystem::path get_scene_fingerprint_output_path(
        const std::string& output_filepath) {
        if (output_filepath.empty()) return "scene_fingerprint.csv";

        std::filesystem::path path = output_filepath;
        const std::filesystem::path parent = path.parent_path();
        const std::string stem = path.stem().string().empty() ? "result" : path.stem().string();
        const std::string extension = path.extension().string().empty() ? ".csv" : path.extension().string();
        return parent / (stem + "_scene_fingerprint" + extension);
    }

    void write_benchmark_csv(
        const std::filesystem::path& path,
        const ProgramArgument& arguments,
        const ProgramResult& result) {

        std::ofstream output(path, std::ios::out | std::ios::trunc);
        if (!output) {
            std::cerr << "Failed to open output file: " << path.string() << '\n';
            return;
        }

        output << ProgramArgument::get_header_string() << ','
            << ProgramResult::get_header_string() << '\n';
        output << arguments.to_string() << ',' << result.to_string() << '\n';

        if (!output) {
            std::cerr << "Failed to write output file: " << path.string() << '\n';
            return;
        }

        std::cout << "Saved CSV: " << path.string() << '\n';
    }

    void write_windowed_benchmark_csv(
        const std::filesystem::path& path,
        const std::array<std::string, util::Constants::MAX_PASS_COUNT>& pass_names,
        const std::vector<FrameCounter::WindowedData>& windows) {

        std::ofstream output(path, std::ios::out | std::ios::trunc);
        if (!output) {
            std::cerr << "Failed to open output file: " << path.string() << '\n';
            return;
        }

        output << "frame";
        for (const auto& name : pass_names)
            if (!name.empty()) output << ',' << name;
        output << '\n' << std::fixed << std::setprecision(5);

        for (const auto& window : windows) {
            output << window.frame;
            for (size_t pass = 0; pass < pass_names.size(); ++pass) {
                if (!pass_names[pass].empty())
                    output << ',' << window.time_avg_ms[pass];
            }
            output << '\n';
        }

        if (!output) {
            std::cerr << "Failed to write output file: " << path.string() << '\n';
            return;
        }

        std::cout << "Saved CSV: " << path.string() << '\n';
    }

}
