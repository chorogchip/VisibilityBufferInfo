#include "scene/SceneResourceBuilder.h"

#include <d3d12.h>
#include <wrl.h>

#include "util/Utils.h"
#include "util/GraphicsUtils.h"

namespace scene {
    std::unique_ptr<SceneDataGPU> SceneResourceBuilder::build(const SceneDataCPU& src,
        ID3D12Device* p_device, ID3D12GraphicsCommandList* p_list, std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>>& used_upload_heaps) {
		std::unique_ptr<SceneDataGPU> ret{ new SceneDataGPU{} };

        struct Material {
            DirectX::XMFLOAT4 base_color;
        };
        std::vector<Material> materials;
        materials.reserve(src.materials.size());
        for (const auto& material : src.materials)
            materials.push_back({ material.base_color });

        Microsoft::WRL::ComPtr<ID3D12Resource> upload_heap_vertex, upload_heap_index, upload_heap_object, upload_heap_material;

        const size_t buf_sz_vertices = src.vertices.size() * sizeof(decltype(src.vertices)::value_type);
        const size_t buf_sz_indices = src.indices.size() * sizeof(decltype(src.indices)::value_type);
        const size_t buf_sz_objects = src.objects.size() * sizeof(decltype(src.objects)::value_type);
        const size_t buf_sz_materials = materials.size() * sizeof(decltype(materials)::value_type);

        assert(0 < buf_sz_vertices && buf_sz_vertices < UINT_MAX);
        assert(0 < buf_sz_indices && buf_sz_indices < UINT_MAX);
        assert(0 < buf_sz_objects && buf_sz_objects < UINT_MAX);
        assert(0 < buf_sz_materials && buf_sz_materials < UINT_MAX);

        GraphicsUtils::create_buffer(upload_heap_vertex, p_device, buf_sz_vertices, 1,
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        GraphicsUtils::create_buffer(upload_heap_index, p_device, buf_sz_indices, 1,
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        GraphicsUtils::create_buffer(upload_heap_object, p_device, buf_sz_objects, 1,
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        GraphicsUtils::create_buffer(upload_heap_material, p_device, buf_sz_materials, 1,
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        GraphicsUtils::create_buffer(ret->vertex_buffer, p_device, buf_sz_vertices, 1,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        GraphicsUtils::create_buffer(ret->index_buffer, p_device, buf_sz_indices, 1,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        GraphicsUtils::create_buffer(ret->object_buffer, p_device, buf_sz_objects, 1,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        GraphicsUtils::create_buffer(ret->material_buffer, p_device, buf_sz_materials, 1,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        
        GraphicsUtils::copy_cpu_to_upload(upload_heap_vertex.Get(), src.vertices.data(), buf_sz_vertices);
        GraphicsUtils::copy_cpu_to_upload(upload_heap_index.Get(), src.indices.data(), buf_sz_indices);
        GraphicsUtils::copy_cpu_to_upload(upload_heap_object.Get(), src.objects.data(), buf_sz_objects);
        GraphicsUtils::copy_cpu_to_upload(upload_heap_material.Get(), materials.data(), buf_sz_materials);

        p_list->CopyBufferRegion(ret->vertex_buffer.Get(), 0, upload_heap_vertex.Get(), 0, buf_sz_vertices);
        p_list->CopyBufferRegion(ret->index_buffer.Get(), 0, upload_heap_index.Get(), 0, buf_sz_indices);
        p_list->CopyBufferRegion(ret->object_buffer.Get(), 0, upload_heap_object.Get(), 0, buf_sz_objects);
        p_list->CopyBufferRegion(ret->material_buffer.Get(), 0, upload_heap_material.Get(), 0, buf_sz_materials);
        
        // COMMON -> COPY_DEST: implicit transition

        GraphicsUtils::record_transition(p_list, ret->vertex_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        GraphicsUtils::record_transition(p_list, ret->index_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);
        GraphicsUtils::record_transition(p_list, ret->object_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        GraphicsUtils::record_transition(p_list, ret->material_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        used_upload_heaps.emplace_back(std::move(upload_heap_vertex));
        used_upload_heaps.emplace_back(std::move(upload_heap_index));
        used_upload_heaps.emplace_back(std::move(upload_heap_object));
        used_upload_heaps.emplace_back(std::move(upload_heap_material));

        ret->vertex_buffer_view.BufferLocation = ret->vertex_buffer->GetGPUVirtualAddress();
        ret->vertex_buffer_view.StrideInBytes = sizeof(SceneDataCPU::Vertex);
        ret->vertex_buffer_view.SizeInBytes = static_cast<UINT>(buf_sz_vertices);

        ret->index_buffer_view.BufferLocation = ret->index_buffer->GetGPUVirtualAddress();
        ret->index_buffer_view.Format = DXGI_FORMAT_R32_UINT; static_assert(sizeof(SceneDataCPU::Index) == 4);
        ret->index_buffer_view.SizeInBytes = static_cast<UINT>(buf_sz_indices);

		return ret;
	}
}
