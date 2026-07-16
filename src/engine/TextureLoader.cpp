#include "engine/TextureLoader.h"

#include 

namespace eng {

	static struct TextureLoaderData {
		ID3D12Device* device;
		ID3D12CommandQueue* queue;
		ID3D12DescriptorHeap* srv_heap;
		uint32_t srv_heap_desc_size;
		uint32_t srv_heap_start;
	};
	static TextureLoaderData data_;

	void init(
		ID3D12Device* device,
		ID3D12CommandQueue* queue,
		ID3D12DescriptorHeap* srv_heap,
		uint32_t srv_index_start) {


		data_.device = device;
		data_.queue = queue;
		data_.srv_heap = srv_heap;
		data_.srv_heap_desc_size = device->GetDescriptorHandleIncrementSize(
			D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		data_.srv_heap_start = srv_index_start;
	}
	static void close() {

	}

	eng::TextureHandle TextureLoader::load_texture(std::filesystem::path path) {
		
		// device (wic?)


	}

	eng::Texture& get_texture(eng::TextureHandle handle) {

	}
}

