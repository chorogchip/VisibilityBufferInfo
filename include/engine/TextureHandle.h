#pragma once

namespace eng {

	class TextureHandle {
	private:
		TextureHandle(uint32_t id_) : id{ id_ };
		TextureHandle(const TextureHandle&) = default;
		TextureHandle& operator=(const TextureHandle&) = default;
		uint32_t id;
	public:
		uint32_t get() const { return id; }
		friend class TextureLoader;
	};
}