#pragma once

#include <cstdint>
#include <Windows.h>

namespace core {
	class AppWindow {

	public:
		HWND hwnd;
		uint32_t width() const noexcept { return width_;  }
		uint32_t height() const noexcept { return height_;  }

	private:
		uint32_t width_, height_;
	};
}