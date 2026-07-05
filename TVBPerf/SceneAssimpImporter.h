#pragma once

#include "ImportedScene.h"

#include <filesystem>

ImportedSceneLoadResult load_imported_scene_with_assimp(const std::filesystem::path& path);
