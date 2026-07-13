#include "dx_util/RendererBindings.h"

#include "util/Utils.h"
#include "dx_util/DescriptorUtils.h"

#include "util/Macros.h"

namespace dxutl {

	void RendererBindings::build(
        ID3D12Device* device) {

        // srv heap

        if (!srv_descs_.empty()) {

            srv_heap_ = dxutl::create_descriptor_heap(
                device,
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                src_heap_max_size_,
                D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
                "create srv descriptor heap");

            srv_descriptor_size_ = device->GetDescriptorHandleIncrementSize(
                D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            for (size_t i = 0; i < srv_descs_.size(); ++i) {

                D3D12_CPU_DESCRIPTOR_HANDLE dest_handle =
                    srv_heap_->GetCPUDescriptorHandleForHeapStart();
                dest_handle.ptr += srv_descs_[i].index * srv_descriptor_size_;

                device->CreateShaderResourceView(
                    srv_descs_[i].resource, &srv_descs_[i].src_desc, dest_handle);
            }
        }

        // root signature

        D3D12_ROOT_SIGNATURE_DESC root_sig_desc{};
        root_sig_desc.NumParameters = static_cast<UINT>(root_parameters_.size());
        root_sig_desc.pParameters = root_parameters_.data();
        root_sig_desc.NumStaticSamplers = 0;
        root_sig_desc.pStaticSamplers = nullptr;
        root_sig_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Microsoft::WRL::ComPtr<ID3DBlob> signature;
        Microsoft::WRL::ComPtr<ID3DBlob> error;

        Utils::throw_if_failed(D3D12SerializeRootSignature(
            &root_sig_desc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature,
            &error),
            "create root signature");

        Utils::throw_if_failed(device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(root_signature_.ReleaseAndGetAddressOf())),
            "create root signature");
	}

    void RendererBindings::add_descriptor(
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        D3D12_SRV_DIMENSION dimention,
        UINT heap_index) {

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = format;
        srv_desc.ViewDimension = dimention;
        srv_desc.Texture2D.MipLevels = 1;

        srv_descs_.emplace_back(srv_desc, resource, heap_index);

        src_heap_max_size_ = std::max(src_heap_max_size_, heap_index + 1);
    }
}
