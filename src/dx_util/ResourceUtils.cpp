#include "dx_util/ResourceUtils.h"

#include <cstring>

#include "util/Utils.h"

namespace dxutl {

    Microsoft::WRL::ComPtr<ID3D12Resource> create_committed_resource(
        ID3D12Device* device,
        const D3D12_RESOURCE_DESC& resource_desc,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_STATES initial_state,
        const D3D12_CLEAR_VALUE* clear_value) {

        D3D12_HEAP_PROPERTIES heap_props{};
        heap_props.Type = heap_type;
        heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heap_props.CreationNodeMask = 1;
        heap_props.VisibleNodeMask = 1;

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        util::Utils::throw_if_failed(device->CreateCommittedResource(
            &heap_props,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            initial_state,
            clear_value,
            IID_PPV_ARGS(resource.ReleaseAndGetAddressOf())),
            "create committed resource");

        return resource;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> create_buffer(
        ID3D12Device* device,
        UINT64 size_in_bytes,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_STATES initial_state) {

        D3D12_RESOURCE_DESC resource_desc{};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resource_desc.Alignment = 0;
        resource_desc.Width = size_in_bytes;
        resource_desc.Height = 1;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.SampleDesc.Quality = 0;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        return create_committed_resource(device, resource_desc, heap_type, initial_state);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture2d(
        ID3D12Device* device,
        UINT64 width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_STATES initial_state,
        D3D12_RESOURCE_FLAGS flags,
        const D3D12_CLEAR_VALUE* clear_value) {

        D3D12_RESOURCE_DESC resource_desc{};
        resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resource_desc.Alignment = 0;
        resource_desc.Width = width;
        resource_desc.Height = height;
        resource_desc.DepthOrArraySize = 1;
        resource_desc.MipLevels = 1;
        resource_desc.Format = format;
        resource_desc.SampleDesc.Count = 1;
        resource_desc.SampleDesc.Quality = 0;
        resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resource_desc.Flags = flags;

        return create_committed_resource(device, resource_desc, heap_type, initial_state, clear_value);
    }


    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture2d_array(
        ID3D12Device* device,
        UINT64 width,
        UINT height,
        UINT16 array_size,
        DXGI_FORMAT format,
        D3D12_RESOURCE_STATES initial_state,
        D3D12_RESOURCE_FLAGS flags,
        const D3D12_CLEAR_VALUE* clear_value) {

        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = array_size;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = flags;

        return dxutl::create_committed_resource(
            device, desc, D3D12_HEAP_TYPE_DEFAULT, initial_state, clear_value);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture2d_fallback(
        ID3D12Device* device,
        DXGI_FORMAT format) {

        return dxutl::create_texture2d(
            device, 1, 1, format,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture2d_array_fallback(
        ID3D12Device* device,
        DXGI_FORMAT format) {

        return dxutl::create_texture2d_array(
            device, 1, 1, 1, format,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture2d_cubemap_fallback(
        ID3D12Device* device,
        DXGI_FORMAT format) {

        return dxutl::create_texture2d_array(
            device, 1, 1, 6, format,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    }



    Microsoft::WRL::ComPtr<ID3D12Resource> create_depth_stencil_buffer(
        ID3D12Device* device,
        UINT64 width,
        UINT height,
        DXGI_FORMAT format,
        D3D12_RESOURCE_STATES initial_state,
        float clear_depth,
        UINT8 clear_stencil) {

        D3D12_RESOURCE_DESC depth_desc{};
        depth_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depth_desc.Alignment = 0;
        depth_desc.Width = width;
        depth_desc.Height = height;
        depth_desc.DepthOrArraySize = 1;
        depth_desc.MipLevels = 1;
        depth_desc.Format = format;
        depth_desc.SampleDesc.Count = 1;
        depth_desc.SampleDesc.Quality = 0;
        depth_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depth_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clear_value{};
        clear_value.Format = format;
        clear_value.DepthStencil.Depth = clear_depth;
        clear_value.DepthStencil.Stencil = clear_stencil;

        return create_committed_resource(
            device, depth_desc, D3D12_HEAP_TYPE_DEFAULT, initial_state, &clear_value);
    }

    void create_depth_stencil_view(
        ID3D12Device* device,
        ID3D12Resource* depth_stencil_buffer,
        DXGI_FORMAT format,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle,
        D3D12_DSV_FLAGS flags) {

        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.Format = format;
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Flags = flags;
        dsv_desc.Texture2D.MipSlice = 0;

        device->CreateDepthStencilView(depth_stencil_buffer, &dsv_desc, dsv_handle);
    }

    void copy_to_upload_buffer(
        ID3D12Resource* upload_buffer,
        const void* source,
        size_t size_in_bytes) {

        void* mapped_data = nullptr;
        D3D12_RANGE read_range{};
        read_range.Begin = 0;
        read_range.End = 0;

        util::Utils::throw_if_failed(upload_buffer->Map(0, &read_range, &mapped_data), "map upload buffer");
        std::memcpy(mapped_data, source, size_in_bytes);
        upload_buffer->Unmap(0, nullptr);
    }

    void* map_upload_buffer(ID3D12Resource* upload_buffer) {
        void* mapped_data = nullptr;
        D3D12_RANGE read_range{};
        read_range.Begin = 0;
        read_range.End = 0;

        util::Utils::throw_if_failed(upload_buffer->Map(0, &read_range, &mapped_data), "map upload buffer");
        return mapped_data;
    }

    void transition_resource(
        ID3D12GraphicsCommandList* command_list,
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES before,
        D3D12_RESOURCE_STATES after,
        UINT subresource) {

        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = subresource;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;

        command_list->ResourceBarrier(1, &barrier);
    }
}
