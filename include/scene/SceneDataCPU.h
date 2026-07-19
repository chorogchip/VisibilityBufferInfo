#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <DirectXMath.h>

#include "engine/MaterialCPU.h"

namespace scene {

    struct SceneDataCPU {

        struct Vertex {
            DirectX::XMFLOAT3 position{};
            DirectX::XMFLOAT3 normal{};
            DirectX::XMFLOAT2 uv0{}, uv1{};
            DirectX::XMFLOAT3 tangent{};
        };

        using Index = uint32_t;

        struct Mesh {
            std::string name;
            uint32_t vertex_start = 0;
            uint32_t vertex_count = 0;
            uint32_t index_start = 0;
            uint32_t index_count = 0;
        };

        struct Object {
            uint32_t object_id = 0;
            uint32_t material_index = 0;
            uint32_t mesh_index = 0;
            uint32_t flags;
            DirectX::XMFLOAT4X4 transform;
        };
        static_assert(sizeof(Object) == 80);

        struct ObjectBatch {
            uint32_t object_index = 0;
            uint32_t object_count = 0;
            uint32_t material_index = 0;
            uint32_t mesh_index = 0;
        };

        std::filesystem::path source_path;
        bool loaded = false;
        std::string error_message;

        std::vector<eng::MaterialCPU> materials;
        std::vector<Vertex> vertices;
        std::vector<Index> indices;
        std::vector<Mesh> meshes;
        std::vector<Object> objects;
        std::vector<ObjectBatch> batches;
        DirectX::XMFLOAT3 bounds_min{};
        DirectX::XMFLOAT3 bounds_max{};

        void build_random_material(size_t material_count);
        void build_batches_from_objects();
    };
}
