#pragma once

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ImportedSceneVertex
{
    DirectX::XMFLOAT3 position = {};
    DirectX::XMFLOAT3 normal = {};
    DirectX::XMFLOAT2 uv = {};
    DirectX::XMFLOAT4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    uint32_t material_index = 0;
};

struct ImportedSceneMesh
{
    std::string name;
    uint32_t vertex_start = 0;
    uint32_t vertex_count = 0;
    uint32_t index_start = 0;
    uint32_t index_count = 0;
    uint32_t material_index = 0;
};

struct ImportedSceneMaterial
{
    std::string name;
    DirectX::XMFLOAT4 base_color = {1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<std::filesystem::path> texture_paths;
};

struct ImportedScene
{
    std::filesystem::path source_path;
    std::vector<ImportedSceneVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<ImportedSceneMesh> meshes;
    std::vector<ImportedSceneMaterial> materials;
    DirectX::XMFLOAT3 bounds_min = {};
    DirectX::XMFLOAT3 bounds_max = {};
};

struct ImportedSceneLoadResult
{
    bool loaded = false;
    ImportedScene scene;
    std::string message;
};
