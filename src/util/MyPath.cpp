#include "util/MyPath.h"

#include <filesystem>
#include <string>

namespace util {

	MyPath MyPath::get_absolute() const {
		return path_.is_absolute() ?
			path_.lexically_normal() :
			std::filesystem::absolute(path_).lexically_normal();
	}

	bool MyPath::is_lower_contain(const char* str) const {
		std::string value = path_.generic_string();
		for (auto& ch : value) ch = std::tolower(ch);
		return value.find(str) != std::string::npos;
	}

	bool MyPath::is_extention_lower_contain(const char* str) const {
		std::string extension = path_.extension().string();
		for (auto& ch : extension) ch = std::tolower(ch);
		return extension.find(str) != std::string::npos;
	}

	bool MyPath::is_regular() const {
		return std::filesystem::is_regular_file(path_);
	}
}