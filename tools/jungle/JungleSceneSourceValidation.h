#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

namespace jungle::validation {

    bool validate_package_set(
        const std::vector<std::filesystem::path>& package_paths,
        std::ostream& output,
        std::string& error_message);
}
