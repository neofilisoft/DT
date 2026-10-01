// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ModelLoader.h re-uses Vertex / SkinnedVertex defined in GPUMesh.h
// so there is exactly one definition in the entire codebase.
#include "renderer/resource/GPUMesh.h"
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// ModelLoader.h
//
// Converts .gltf / .glb / .obj / .fbx source files into an intermediate
// CookedModel structure, which MeshRegistry then uploads to the GPU.
//
// Rules:
//   - Zero hardcoded paths. Caller supplies the absolute path obtained via
//     FileSystem::GetProjectAssetDir() + relative_path.
//   - Modular: only links assimp when renderer module is compiled in.
// ---------------------------------------------------------------------------

namespace lacrima::renderer
{
    // ------------------------------------------------------------------
    // Cooked sub-mesh (CPU side, before GPU upload)
    // ------------------------------------------------------------------

    struct CookedSubMesh
    {
        std::vector<Vertex>        vertices;
        std::vector<uint32_t>      indices;

        /// Optional: filled when mesh has skinning data (Step 3).
        std::vector<SkinnedVertex> skinnedVertices;
        bool                       isSkinned = false;

        /// Material index into the parent CookedModel's materials list.
        uint32_t                   materialIndex = 0;
        std::string                name;
    };

    struct CookedMaterial
    {
        std::string albedoTexturePath;
        std::string normalTexturePath;
        std::string mrTexturePath;      // metallic-roughness (B=metallic, G=roughness)
        std::string aoTexturePath;
        std::string emissiveTexturePath;

        float baseColorFactor[4] = {1.f, 1.f, 1.f, 1.f};
        float metallicFactor     = 0.f;
        float roughnessFactor    = 0.5f;
    };

    struct CookedModel
    {
        std::vector<CookedSubMesh>  subMeshes;
        std::vector<CookedMaterial> materials;
        std::string                 sourceFilePath; // for hot-reload
    };

    // ------------------------------------------------------------------
    // ModelLoader
    // ------------------------------------------------------------------

    class ModelLoader
    {
    public:
        ModelLoader()  = default;
        ~ModelLoader() = default;

        ModelLoader(const ModelLoader&)            = delete;
        ModelLoader& operator=(const ModelLoader&) = delete;

        /// Load any supported format (.gltf, .glb, .obj, .fbx, etc.).
        /// @param absolutePath  Full path supplied by caller via FileSystem API.
        bool Load(const std::string& absolutePath, CookedModel& outModel);
        bool LoadGLTF(const std::string& absolutePath, CookedModel& outModel);
        bool LoadAssimp(const std::string& absolutePath, CookedModel& outModel);

    private:
        void ProcessAssimpScene(const void* aiScenePtr, CookedModel& outModel);
        void ExtractMeshData(const void* aiMeshPtr, CookedSubMesh& outSub);
        void ExtractMaterials(const void* aiScenePtr,
                              const std::string& modelDir,
                              CookedModel& outModel);
    };

} // namespace lacrima::renderer
