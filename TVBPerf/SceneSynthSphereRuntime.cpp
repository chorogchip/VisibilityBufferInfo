#include "SceneSynthSphereRuntime.h"

#include <cassert>
#include <DirectXMath.h>

#include "Utils.h"
#include "GraphicsUtils.h"

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

    std::vector<Vertex> vertex_buffer_cpu;
    std::vector<Index> index_buffer_cpu;

    for (const auto& geom : scene.geometries) {
        MeshBufferHandle handle{};
        handle.index_count = geom.indices.size();
        handle.start_index_offset = index_buffer_cpu.size();
        handle.base_vertex_offset = vertex_buffer_cpu.size();
        ret->geometries_handles.push_back(handle);

        for (const auto& v : geom.vertices) vertex_buffer_cpu.push_back(v);
        for (const auto& i : geom.indices) index_buffer_cpu.push_back(i);
    }

    const UINT64 vertex_buffer_size = vertex_buffer_cpu.size() * sizeof(Vertex);
    const UINT64 index_buffer_size = index_buffer_cpu.size() * sizeof(Index);
    ret->vertex_buffer_size = vertex_buffer_size;
    ret->index_buffer_size = index_buffer_size;

    assert(!scene.geometries.empty());
    assert(vertex_buffer_cpu.size() > 0);
    assert(index_buffer_cpu.size() > 0);
    assert(vertex_buffer_size < UINT_MAX);
    assert(index_buffer_size < UINT_MAX);
    
    GraphicsUtils::create_buffer(ret->vertex_buffer, p_device, vertex_buffer_size, 1,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    GraphicsUtils::create_buffer(ret->index_buffer, p_device, index_buffer_size, 1,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    
    GraphicsUtils::copy_cpu_to_upload(ret->vertex_buffer.Get(), vertex_buffer_cpu.data(), vertex_buffer_size);
    GraphicsUtils::copy_cpu_to_upload(ret->index_buffer.Get(), index_buffer_cpu.data(), index_buffer_size);

    return ret;
}
