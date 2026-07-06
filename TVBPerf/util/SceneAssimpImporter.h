#pragma once

#include <filesystem>
#include <memory>

#include "scene/ImportedScene.h"
#include "scene/SceneSynthSphere.h"

std::unique_ptr<ImportedSceneLoadResult> load_imported_scene_with_assimp(const std::filesystem::path& path);
std::unique_ptr<SceneSynthSphere>scene_loaded_scene_to_cpu_scene(const ImportedSceneLoadResult& src);
