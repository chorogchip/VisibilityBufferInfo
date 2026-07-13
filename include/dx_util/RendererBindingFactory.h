#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <vector>

#include "util/RendererInfos.h"
#include "util/ProgramArgument.h"

namespace dxutl {
	
	struct RenderBindings {

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srv_heap;
		UINT srv_heap_size;
		UINT srv_heap_elem_size;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_prepass;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_resolve;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_lighting;
	};

	struct RenderResourceForBindings {
		ID3D12Resource* const vertices = nullptr;
		ID3D12Resource* const indices = nullptr;
		ID3D12Resource* const meshes = nullptr;
		ID3D12Resource* const objects = nullptr;
		ID3D12Resource* const materials = nullptr;
		ID3D12Resource* const visbuf = nullptr;
		std::vector<ID3D12Resource*> textures;
		std::vector<ID3D12Resource*> gbuffers;
	};

	RenderBindings build_bindings(
		ID3D12Device* device,
		const util::ProgramArgument& arguments,
		util::RenderVariant variant,
		RenderResourceForBindings& bindings);
}