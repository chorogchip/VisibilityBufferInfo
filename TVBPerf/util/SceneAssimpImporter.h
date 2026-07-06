#pragma once

#include <filesystem>
#include <memory>

#include "scene/ImportedScene.h"
#include "scene/SceneSynthSphereRuntime.h"

std::unique_ptr<ImportedSceneLoadResult> load_imported_scene_with_assimp(const std::filesystem::path& path);
std::unique_ptr<SceneSynthSphereRuntime>scene_loaded_scene_to_gpu_scene(const ImportedSceneLoadResult& src);
