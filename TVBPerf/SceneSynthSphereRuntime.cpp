#include "SceneSynthSphereRuntime.h"

#include <cassert>
#include <DirectXMath.h>

#include "Utils.h"

std::unique_ptr<SceneSynthSphereRuntime> SceneSynthSphereRuntime::generate(const SceneSynthSphere& scene, ID3D12Device* p_device) {
    std::unique_ptr<SceneSynthSphereRuntime> ret{ new SceneSynthSphereRuntime() };

    for (const auto& inst : scene.instances) {
        auto mat{
            DirectX::XMMatrixScaling(inst.radius, inst.radius, inst.radius) *
            DirectX::XMMatrixTranslation(inst.center.x,inst.center.y, inst.center.z)
        };

        InstanceData data{};
        DirectX::XMStoreFloat4x4(&data.mat_world, DirectX::XMMatrixTranspose(mat));

        ret->instances_datas.push_back(data);
    }

    using Vertex = decltype(scene.geometries[0].vertices)::value_type;
    using Index = decltype(scene.geometries[0].indices)::value_type;

    UINT vertex_count = 0;
    UINT index_count = 0;

    for (const auto& geom : scene.geometries) {
        assert(geom.vertices.size() <= UINT_MAX - vertex_count);  // prevent overflow
        assert(geom.indices.size() <= UINT_MAX - index_count);

        MeshBufferHandle handle{};
        handle.index_count = static_cast<UINT>(geom.indices.size());
        handle.start_index_offset = index_count;
        handle.base_vertex_offset = vertex_count;
        ret->geometries_handles.push_back(handle);

        vertex_count += static_cast<UINT>(geom.vertices.size());
        index_count += static_cast<UINT>(geom.indices.size());
    }

    assert(!scene.geometries.empty());
    assert(vertex_count > 0);
    assert(index_count > 0);

    const UINT64 vertex_buffer_size = static_cast<UINT64>(vertex_count) * sizeof(Vertex);
    const UINT64 index_buffer_size = static_cast<UINT64>(index_count) * sizeof(Index);

    D3D12_HEAP_PROPERTIES heap_props{};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;  // TODO switch to DEFAULT
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Width = vertex_buffer_size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Utils::throw_if_failed(p_device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ret->vertex_buffer)));

    void* mapped_data = nullptr;

    D3D12_RANGE read_range{};
    read_range.Begin = 0;
    read_range.End = 0;

    Utils::throw_if_failed(ret->vertex_buffer->Map(0, &read_range, &mapped_data));

    for (const auto& geom : scene.geometries) {
        const size_t sz = geom.vertices.size() * sizeof(geom.vertices[0]);
        if (sz == 0) continue;

        memcpy(mapped_data, geom.vertices.data(), sz);
        mapped_data = static_cast<char*>(mapped_data) + sz;
    }

    ret->vertex_buffer->Unmap(0, nullptr);

    ret->vertex_buffer_view.BufferLocation = ret->vertex_buffer->GetGPUVirtualAddress();
    ret->vertex_buffer_view.StrideInBytes = sizeof(Vertex);
    assert(vertex_buffer_size < UINT_MAX);
    ret->vertex_buffer_view.SizeInBytes = static_cast<UINT>(vertex_buffer_size);

    resource_desc.Width = index_buffer_size;

    Utils::throw_if_failed(p_device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&ret->index_buffer)));

    mapped_data = nullptr;

    Utils::throw_if_failed(ret->index_buffer->Map(0, &read_range, &mapped_data));

    for (const auto& geom : scene.geometries) {
        const size_t sz = geom.indices.size() * sizeof(Index);
        memcpy(mapped_data, geom.indices.data(), sz);
        mapped_data = static_cast<char*>(mapped_data) + sz;
    }

    ret->index_buffer->Unmap(0, nullptr);

    ret->index_buffer_view.BufferLocation = ret->index_buffer->GetGPUVirtualAddress();
    static_assert(sizeof(Index) == 4, "invalid format");
    ret->index_buffer_view.Format = DXGI_FORMAT_R32_UINT;
    assert(index_buffer_size < UINT_MAX);
    ret->index_buffer_view.SizeInBytes = static_cast<UINT>(index_buffer_size);

    return ret;
}
