#include "scene/donut/DonutSceneImporter.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "util/Logger.h"
#include "scene/Material.h"
#include "scene/Light.h"

namespace scene {

	void DonutScenImporter::load(const std::filesystem::path& path) {

        Assimp::Importer importer;
        const unsigned int flags =
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace |
            aiProcess_ImproveCacheLocality |
            aiProcess_PreTransformVertices |
            aiProcess_SortByPType |
            aiProcess_MakeLeftHanded |
            aiProcess_FlipUVs |
            aiProcess_FlipWindingOrder |
            0;

        const aiScene* assimp_scene = importer.ReadFile(path.string(), flags);

        if (assimp_scene == nullptr || assimp_scene->mNumMeshes == 0) {
            util::Logger::g_logger <<
                "Error in Assimp Scene import: \n\t" <<
                importer.GetErrorString();

            util::Logger::g_logger.assert_with_log(false);
            return;
        }
        const auto& name = assimp_scene->mName;
        const auto flags_out = assimp_scene->mFlags;
        const auto metadata = assimp_scene->mMetaData;
        (void)metadata;

        util::Logger::g_logger << "[Assimp]: scene [" << name.C_Str() << "] load succeed\n";

        util::Logger::g_logger.assert_with_log(
            !(flags_out & AI_SCENE_FLAGS_INCOMPLETE),
            "scene incomplete flag");

        const auto root_node = assimp_scene->mRootNode;
        
        const auto meshes_cnt = assimp_scene->mNumMeshes;
        const auto meshes = assimp_scene->mMeshes;

        const auto materials_cnt = assimp_scene->mNumMaterials;
        const auto materials = assimp_scene->mMaterials;

        const auto animations_cnt = assimp_scene->mNumAnimations;
        const auto animations = assimp_scene->mAnimations;
        (void)animations_cnt;
        (void)animations;

        const auto textures_cnt = assimp_scene->mNumTextures;
        const auto textures = assimp_scene->mTextures;

        const auto lights_cnt = assimp_scene->mNumLights;
        const auto lights = assimp_scene->mLights;
        auto lights_vec = Light::from_assimp(lights, lights_cnt);

        const auto cameras_cnt = assimp_scene->mNumCameras;
        const auto cameras = assimp_scene->mCameras;
        (void)cameras_cnt;
        (void)cameras;

        const auto skeletons_cnt = assimp_scene->mNumSkeletons;
        const auto skeletons = assimp_scene->mSkeletons;
        (void)skeletons_cnt;
        (void)skeletons;
	}
}
