#include "render/pass/PassDescriptorRequests.h"

#include "engine/MaterialGPU.h"

namespace rndr {
    void request_material_textures(
        eng::ResourceManagerShader& manager,
        const std::vector<ID3D12Resource*>& textures) {
        for (UINT i = 0; i < textures.size(); ++i)
            manager.create_srv(
                eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN,
                textures[i], nullptr, i);
    }

    void request_visbuf_scene(
        eng::ResourceManagerShader& manager,
        ID3D12Resource* vertex_buffer,
        ID3D12Resource* index_buffer,
        ID3D12Resource* mesh_buffer,
        ID3D12Resource* instance_buffer,
        ID3D12Resource* material_buffer,
        const std::vector<ID3D12Resource*>& material_textures,
        const scene::SceneDataCPU* scene) {

        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        desc.Buffer.StructureByteStride = sizeof(scene->vertices[0]);
        desc.Buffer.NumElements = static_cast<UINT>(scene->vertices.size());
        manager.create_srv(eng::ResourceManagerShader::EnumDescPos::BENCH_VERTEX_BUFFER,
            vertex_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(scene->indices[0]);
        desc.Buffer.NumElements = static_cast<UINT>(scene->indices.size());
        manager.create_srv(eng::ResourceManagerShader::EnumDescPos::BENCH_INDEX_BUFFER,
            index_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(uint32_t) * 2;
        desc.Buffer.NumElements = static_cast<UINT>(scene->meshes.size());
        manager.create_srv(eng::ResourceManagerShader::EnumDescPos::BENCH_MESH_BUFFER,
            mesh_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(scene->objects[0]);
        desc.Buffer.NumElements = static_cast<UINT>(scene->objects.size());
        manager.create_srv(eng::ResourceManagerShader::EnumDescPos::BENCH_INSTANCE_BUFFER,
            instance_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(eng::MaterialGPU);
        desc.Buffer.NumElements = static_cast<UINT>(scene->materials.size());
        manager.create_srv(eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_BUFFER,
            material_buffer, &desc);
        request_material_textures(manager, material_textures);
    }
}
