#pragma once

#include <Windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

using Microsoft::WRL::ComPtr;

class GraphicsUtils {
	
public:
	static void compile_shader(ID3DBlob** pp_blob, LPCWSTR filename, LPCSTR shader_model);

	static void create_device(ComPtr<IDXGIFactory4>& factory, ComPtr<ID3D12Device>& device);
	static void create_command_objects(ID3D12Device* p_device, ComPtr<ID3D12CommandQueue>& command_queue, ComPtr<ID3D12GraphicsCommandList>& command_list, ComPtr<ID3D12CommandAllocator> allocator_lists[], int allocator_count);
	static void create_swapchain(HWND hwnd, IDXGIFactory4* p_factory, ID3D12Device* p_device, ID3D12CommandQueue* p_command_queue, ComPtr<IDXGISwapChain3>& swap_chain, UINT width, UINT height, UINT frame_count);

	static void create_buffer(ComPtr<ID3D12Resource>& buffer, ID3D12Device* p_device, UINT64 width, UINT height, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_STATES state);
	
	static void copy_cpu_to_upload(ID3D12Resource* dest, const void* src, size_t sz);
	static void* get_mapped_address(ID3D12Resource* dest);

	static void record_transition(ID3D12GraphicsCommandList* p_list, ID3D12Resource* p_resource,
		D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);
};