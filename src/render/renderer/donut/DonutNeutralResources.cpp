#include "render/renderer/donut/DonutNeutralResources.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "dx_util/ResourceUtils.h"
#include "util/Logger.h"
#include "util/Utils.h"

namespace rndr {

    namespace {

        Microsoft::WRL::ComPtr<ID3D12Resource> create_rgba8_texture(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            UINT16 array_size,
            const std::array<uint8_t, 4>& pixel,
            std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = 1;
            texture_desc.Height = 1;
            texture_desc.DepthOrArraySize = array_size;
            texture_desc.MipLevels = 1;
            texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

            auto texture = dxutl::create_committed_resource(
                device,
                texture_desc,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_COPY_DEST);

            std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(array_size);
            std::vector<UINT> row_counts(array_size);
            std::vector<UINT64> row_sizes(array_size);
            UINT64 upload_size = 0;
            device->GetCopyableFootprints(
                &texture_desc,
                0,
                array_size,
                0,
                footprints.data(),
                row_counts.data(),
                row_sizes.data(),
                &upload_size);

            auto upload = dxutl::create_buffer(
                device,
                upload_size,
                D3D12_HEAP_TYPE_UPLOAD,
                D3D12_RESOURCE_STATE_GENERIC_READ);

            auto* mapped = static_cast<std::byte*>(dxutl::map_upload_buffer(upload.Get()));
            for (UINT subresource = 0; subresource < array_size; ++subresource) {
                util::Logger::g_logger.assert_with_log(
                    row_counts[subresource] == 1 && row_sizes[subresource] == pixel.size(),
                    "unexpected Donut neutral texture footprint");
                std::memcpy(
                    mapped + footprints[subresource].Offset,
                    pixel.data(),
                    pixel.size());
            }
            upload->Unmap(0, nullptr);

            for (UINT subresource = 0; subresource < array_size; ++subresource) {
                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource = upload.Get();
                source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                source.PlacedFootprint = footprints[subresource];

                D3D12_TEXTURE_COPY_LOCATION target{};
                target.pResource = texture.Get();
                target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                target.SubresourceIndex = subresource;

                command_list->CopyTextureRegion(&target, 0, 0, 0, &source, nullptr);
            }

            dxutl::transition_resource(
                command_list,
                texture.Get(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

            used_upload_heaps.emplace_back(std::move(upload));
            return texture;
        }

        Microsoft::WRL::ComPtr<ID3D12Resource> create_black_depth_stencil_array(
            ID3D12Device* device,
            ID3D12GraphicsCommandList* command_list,
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& dsv_heap) {

            D3D12_CLEAR_VALUE clear{};
            clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            clear.DepthStencil.Depth = 0.0f;
            clear.DepthStencil.Stencil = 0;

            D3D12_RESOURCE_DESC texture_desc{};
            texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            texture_desc.Width = 1;
            texture_desc.Height = 1;
            texture_desc.DepthOrArraySize = 1;
            texture_desc.MipLevels = 1;
            texture_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;
            texture_desc.SampleDesc.Count = 1;
            texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            texture_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

            auto texture = dxutl::create_committed_resource(
                device,
                texture_desc,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &clear);

            D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            heap_desc.NumDescriptors = 1;

            dsv_heap.Reset();
            util::Utils::throw_if_failed(
                device->CreateDescriptorHeap(
                    &heap_desc,
                    IID_PPV_ARGS(dsv_heap.ReleaseAndGetAddressOf())),
                "create Donut neutral depth DSV heap");

            D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
            dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsv_desc.Texture2DArray.MipSlice = 0;
            dsv_desc.Texture2DArray.FirstArraySlice = 0;
            dsv_desc.Texture2DArray.ArraySize = 1;

            device->CreateDepthStencilView(
                texture.Get(),
                &dsv_desc,
                dsv_heap->GetCPUDescriptorHandleForHeapStart());
            command_list->ClearDepthStencilView(
                dsv_heap->GetCPUDescriptorHandleForHeapStart(),
                D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                0.0f,
                0,
                0,
                nullptr);

            dxutl::transition_resource(
                command_list,
                texture.Get(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

            return texture;
        }
    }

    void DonutNeutralResources::init(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {

        util::Logger::g_logger.assert_with_log(
            device != nullptr && command_list != nullptr,
            "Donut neutral resources require a device and command list");

        black_texture.init(
            create_rgba8_texture(
                device,
                command_list,
                1,
                { 0, 0, 0, 255 },
                used_upload_heaps).Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

        white_texture.init(
            create_rgba8_texture(
                device,
                command_list,
                1,
                { 255, 255, 255, 255 },
                used_upload_heaps).Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

        black_cube_map_array.init(
            create_rgba8_texture(
                device,
                command_list,
                6,
                { 0, 0, 0, 255 },
                used_upload_heaps).Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);

        black_depth_stencil_texture_2d_array.init(
            create_black_depth_stencil_array(
                device,
                command_list,
                depth_clear_dsv_heap).Get(),
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }
}
