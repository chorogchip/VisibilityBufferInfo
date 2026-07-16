#pragma once

#include <filesystem>

#include "engine/Texture.h"
#include "engine/TextureHandle.h"

namespace eng {

	class TextureLoader {

	public:

		static void init(
			ID3D12Device* device,
			ID3D12CommandQueue* queue,
			ID3D12DescriptorHeap* srv_heap,
			uint32_t srv_index_start
		);
		static void close();
		static eng::TextureHandle load_texture(std::filesystem::path path);
		static eng::Texture& get_texture(eng::TextureHandle handle);
	};
}