#pragma once

#include <string>
#include <filesystem>

namespace util {
	class MyPath {

	public:
		MyPath() = default;
		MyPath(const std::filesystem::path& path) : path_{ path } {}
		MyPath(const MyPath&) = default;
		MyPath& operator=(const MyPath&) = default;

		std::filesystem::path get() const { return path_; }
		MyPath get_absolute() const;
		bool is_lower_contain(const char* str) const;
		bool is_extention_lower_contain(const char* str) const;
		bool is_regular() const;

	private:
		std::filesystem::path path_;
	};
}