#include "render/pass/PassDescriptorRequests.h"

#include "engine/MaterialGPU.h"

namespace rndr {
    void request_material_textures(
        eng::ResourceManagerShader& manager,
        const scene::SceneResources& resources) {
        for (UINT i = 0; i < resources.material_textures.size(); ++i)
            manager.request(
                eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_TEXTURE_BEGIN,
                resources.material_textures[i], nullptr, i);
    }

    void request_visbuf_scene(
        eng::ResourceManagerShader& manager,
        const VisBufResources& resources) {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        desc.Buffer.StructureByteStride = sizeof(resources.scene.cpu->vertices[0]);
        desc.Buffer.NumElements = static_cast<UINT>(resources.scene.cpu->vertices.size());
        manager.request(eng::ResourceManagerShader::EnumDescPos::BENCH_VERTEX_BUFFER,
            resources.scene.vertex_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(resources.scene.cpu->indices[0]);
        desc.Buffer.NumElements = static_cast<UINT>(resources.scene.cpu->indices.size());
        manager.request(eng::ResourceManagerShader::EnumDescPos::BENCH_INDEX_BUFFER,
            resources.scene.index_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(uint32_t) * 2;
        desc.Buffer.NumElements = static_cast<UINT>(resources.scene.cpu->meshes.size());
        manager.request(eng::ResourceManagerShader::EnumDescPos::BENCH_MESH_BUFFER,
            resources.mesh_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(resources.scene.cpu->objects[0]);
        desc.Buffer.NumElements = static_cast<UINT>(resources.scene.cpu->objects.size());
        manager.request(eng::ResourceManagerShader::EnumDescPos::BENCH_INSTANCE_BUFFER,
            resources.scene.instance_buffer, &desc);
        desc.Buffer.StructureByteStride = sizeof(eng::MaterialGPU);
        desc.Buffer.NumElements = static_cast<UINT>(resources.scene.cpu->materials.size());
        manager.request(eng::ResourceManagerShader::EnumDescPos::BENCH_MATERIAL_BUFFER,
            resources.scene.material_buffer, &desc);
        request_material_textures(manager, resources.scene);
    }
}
