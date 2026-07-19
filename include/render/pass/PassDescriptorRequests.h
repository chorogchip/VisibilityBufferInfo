#pragma once

#include "engine/ResourceManagerShader.h"
#include <vector>

#include "scene/SceneDataCPU.h"

namespace rndr {
    void request_material_textures(
        eng::ResourceManagerShader& manager,
        const std::vector<ID3D12Resource*>& textures);

    void request_visbuf_scene(
        eng::ResourceManagerShader& manager,
        ID3D12Resource* vertex_buffer,
        ID3D12Resource* index_buffer,
        ID3D12Resource* mesh_buffer,
        ID3D12Resource* instance_buffer,
        ID3D12Resource* material_buffer,
        const std::vector<ID3D12Resource*>& material_textures,
        const scene::SceneDataCPU* scene);
}
