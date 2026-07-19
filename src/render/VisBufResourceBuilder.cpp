#include "render/VisBufResourceBuilder.h"

#include <vector>

#include "dx_util/ResourceUtils.h"
#include "util/Logger.h"

namespace rndr {
    Microsoft::WRL::ComPtr<ID3D12Resource> VisBufResourceBuilder::build_mesh_buffer(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* command_list,
        ID3D12CommandAllocator* command_allocator,
        ID3D12CommandQueue* command_queue,
        dxutl::Fence& fence,
        const scene::SceneResources& scene) {
        struct MeshGPU { uint32_t vertex_start; uint32_t index_start; };
        std::vector<MeshGPU> meshes;
        meshes.reserve(scene.cpu->meshes.size());
        for (const auto& mesh : scene.cpu->meshes)
            meshes.push_back({ mesh.vertex_start, mesh.index_start });

        const size_t size = meshes.size() * sizeof(MeshGPU);
        auto upload = dxutl::create_buffer(device, size,
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
        auto mesh_buffer = dxutl::create_buffer(device, size,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON);
        dxutl::copy_to_upload_buffer(upload.Get(), meshes.data(), size);

        HRESULT result = command_list->Reset(command_allocator, nullptr);
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "reset list for VisBuf resources");
        command_list->CopyBufferRegion(mesh_buffer.Get(), 0, upload.Get(), 0, size);
        dxutl::transition_resource(command_list, mesh_buffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(command_list, scene.vertex_buffer,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(command_list, scene.index_buffer,
            D3D12_RESOURCE_STATE_INDEX_BUFFER,
            D3D12_RESOURCE_STATE_INDEX_BUFFER | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        dxutl::transition_resource(command_list, scene.instance_buffer,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
        result = command_list->Close();
        util::Logger::g_logger.assert_with_log(SUCCEEDED(result), "close list for VisBuf resources");
        ID3D12CommandList* lists[] = { command_list };
        command_queue->ExecuteCommandLists(_countof(lists), lists);
        fence.wait_for_gpu();
        return mesh_buffer;
    }
}
