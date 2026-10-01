// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/resource/ModelLoader.h"
#include "core/logging/Logger.h"

// Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// tinygltf (header-only, compiled here once)
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tinygltf/tiny_gltf.h>

#include <filesystem>
#include <algorithm>
#include <functional>

namespace lacrima::renderer
{

bool ModelLoader::Load(const std::string& absolutePath, CookedModel& outModel)
{
    if (absolutePath.empty())
    {
        LACRIMA_LOG_ERROR(lacrima::LogCategory::Renderer, "[ModelLoader] absolutePath is empty.");
        return false;
    }

    std::filesystem::path p(absolutePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".gltf" || ext == ".glb")
        return LoadGLTF(absolutePath, outModel);

    return LoadAssimp(absolutePath, outModel);
}

bool ModelLoader::LoadGLTF(const std::string& absolutePath, CookedModel& outModel)
{
    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string        err, warn;

    std::filesystem::path p(absolutePath);
    bool ok = false;

    if (p.extension() == ".glb")
        ok = loader.LoadBinaryFromFile(&model, &err, &warn, absolutePath);
    else
        ok = loader.LoadASCIIFromFile(&model, &err, &warn, absolutePath);

    if (!warn.empty())
        LACRIMA_LOG_WARN(lacrima::LogCategory::Renderer, "[ModelLoader] tinygltf warn: {}", warn);

    if (!ok)
    {
        LACRIMA_LOG_ERROR(lacrima::LogCategory::Renderer, "[ModelLoader] tinygltf failed to load {}: {}", absolutePath, err);
        return false;
    }

    outModel.sourceFilePath = absolutePath;
    std::string modelDir = p.parent_path().string();

    // Materials
    outModel.materials.reserve(model.materials.size());
    for (const auto& mat : model.materials)
    {
        CookedMaterial cm;
        auto resolveTexture = [&](int texIdx) -> std::string {
            if (texIdx < 0) return {};
            const auto& src = model.images[model.textures[texIdx].source];
            if (!src.uri.empty())
                return (std::filesystem::path(modelDir) / src.uri).string();
            return {};
        };

        cm.albedoTexturePath   = resolveTexture(mat.pbrMetallicRoughness.baseColorTexture.index);
        cm.mrTexturePath       = resolveTexture(mat.pbrMetallicRoughness.metallicRoughnessTexture.index);
        cm.normalTexturePath   = resolveTexture(mat.normalTexture.index);
        cm.aoTexturePath       = resolveTexture(mat.occlusionTexture.index);
        cm.emissiveTexturePath = resolveTexture(mat.emissiveTexture.index);

        const auto& bcf = mat.pbrMetallicRoughness.baseColorFactor;
        if (bcf.size() == 4)
        {
            cm.baseColorFactor[0] = static_cast<float>(bcf[0]);
            cm.baseColorFactor[1] = static_cast<float>(bcf[1]);
            cm.baseColorFactor[2] = static_cast<float>(bcf[2]);
            cm.baseColorFactor[3] = static_cast<float>(bcf[3]);
        }
        cm.metallicFactor  = static_cast<float>(mat.pbrMetallicRoughness.metallicFactor);
        cm.roughnessFactor = static_cast<float>(mat.pbrMetallicRoughness.roughnessFactor);

        outModel.materials.push_back(std::move(cm));
    }

    // Meshes
    for (const auto& mesh : model.meshes)
    {
        for (const auto& prim : mesh.primitives)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;

            CookedSubMesh sub;
            sub.name          = mesh.name;
            sub.materialIndex = (prim.material >= 0) ? static_cast<uint32_t>(prim.material) : 0u;

            auto getAttr = [&](const std::string& attr) -> const tinygltf::Accessor* {
                auto it = prim.attributes.find(attr);
                if (it == prim.attributes.end()) return nullptr;
                return &model.accessors[it->second];
            };

            const auto* posAcc  = getAttr("POSITION");
            const auto* normAcc = getAttr("NORMAL");
            const auto* uvAcc   = getAttr("TEXCOORD_0");
            const auto* tanAcc  = getAttr("TANGENT");

            if (!posAcc) continue;
            size_t vertCount = posAcc->count;
            sub.vertices.resize(vertCount);

            auto readVec3 = [&](const tinygltf::Accessor& acc, size_t i, float* out) {
                const auto& bv  = model.bufferViews[acc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                size_t stride = bv.byteStride ? bv.byteStride : sizeof(float) * 3;
                const float* src = reinterpret_cast<const float*>(buf.data.data() + bv.byteOffset + acc.byteOffset + i * stride);
                out[0] = src[0]; out[1] = src[1]; out[2] = src[2];
            };
            auto readVec2 = [&](const tinygltf::Accessor& acc, size_t i, float* out) {
                const auto& bv  = model.bufferViews[acc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                size_t stride = bv.byteStride ? bv.byteStride : sizeof(float) * 2;
                const float* src = reinterpret_cast<const float*>(buf.data.data() + bv.byteOffset + acc.byteOffset + i * stride);
                out[0] = src[0]; out[1] = src[1];
            };
            auto readVec4 = [&](const tinygltf::Accessor& acc, size_t i, float* out) {
                const auto& bv  = model.bufferViews[acc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                size_t stride = bv.byteStride ? bv.byteStride : sizeof(float) * 4;
                const float* src = reinterpret_cast<const float*>(buf.data.data() + bv.byteOffset + acc.byteOffset + i * stride);
                out[0] = src[0]; out[1] = src[1]; out[2] = src[2]; out[3] = src[3];
            };

            for (size_t i = 0; i < vertCount; ++i)
            {
                Vertex& v = sub.vertices[i];
                readVec3(*posAcc, i, v.position);
                if (normAcc) readVec3(*normAcc, i, v.normal);
                if (uvAcc)   readVec2(*uvAcc,   i, v.texCoord);
                if (tanAcc)  readVec4(*tanAcc,  i, v.tangent);
            }

            if (prim.indices >= 0)
            {
                const auto& idxAcc = model.accessors[prim.indices];
                sub.indices.resize(idxAcc.count);
                const auto& bv  = model.bufferViews[idxAcc.bufferView];
                const auto& buf = model.buffers[bv.buffer];
                const uint8_t* rawData = buf.data.data() + bv.byteOffset + idxAcc.byteOffset;

                for (size_t i = 0; i < idxAcc.count; ++i)
                {
                    if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        sub.indices[i] = reinterpret_cast<const uint16_t*>(rawData)[i];
                    else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                        sub.indices[i] = reinterpret_cast<const uint32_t*>(rawData)[i];
                    else
                        sub.indices[i] = rawData[i];
                }
            }

            outModel.subMeshes.push_back(std::move(sub));
        }
    }

    LACRIMA_LOG_INFO(lacrima::LogCategory::Renderer, "[ModelLoader] GLTF loaded: {} - {} subMeshes", absolutePath, outModel.subMeshes.size());
    return true;
}

bool ModelLoader::LoadAssimp(const std::string& absolutePath, CookedModel& outModel)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        absolutePath,
        aiProcess_Triangulate | aiProcess_GenNormals |
        aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices |
        aiProcess_SortByPType);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        LACRIMA_LOG_ERROR(lacrima::LogCategory::Renderer, "[ModelLoader] Assimp error for {}: {}", absolutePath, importer.GetErrorString());
        return false;
    }

    outModel.sourceFilePath = absolutePath;
    std::string modelDir = std::filesystem::path(absolutePath).parent_path().string();

    ExtractMaterials(scene, modelDir, outModel);
    ProcessAssimpScene(scene, outModel);

    LACRIMA_LOG_INFO(lacrima::LogCategory::Renderer, "[ModelLoader] Assimp loaded: {} - {} subMeshes", absolutePath, outModel.subMeshes.size());
    return true;
}

void ModelLoader::ProcessAssimpScene(const void* aiScenePtr, CookedModel& outModel)
{
    const aiScene* scene = static_cast<const aiScene*>(aiScenePtr);
    std::function<void(aiNode*)> traverse = [&](aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            CookedSubMesh sub;
            ExtractMeshData(mesh, sub);
            sub.materialIndex = mesh->mMaterialIndex;
            sub.name          = mesh->mName.C_Str();
            outModel.subMeshes.push_back(std::move(sub));
        }
        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            traverse(node->mChildren[i]);
    };
    traverse(scene->mRootNode);
}

void ModelLoader::ExtractMeshData(const void* aiMeshPtr, CookedSubMesh& outSub)
{
    const aiMesh* mesh = static_cast<const aiMesh*>(aiMeshPtr);
    outSub.vertices.resize(mesh->mNumVertices);

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex& v = outSub.vertices[i];
        v.position[0] = mesh->mVertices[i].x;
        v.position[1] = mesh->mVertices[i].y;
        v.position[2] = mesh->mVertices[i].z;

        if (mesh->HasNormals())
        {
            v.normal[0] = mesh->mNormals[i].x;
            v.normal[1] = mesh->mNormals[i].y;
            v.normal[2] = mesh->mNormals[i].z;
        }

        if (mesh->mTextureCoords[0])
        {
            v.texCoord[0] = mesh->mTextureCoords[0][i].x;
            v.texCoord[1] = mesh->mTextureCoords[0][i].y;
        }

        if (mesh->HasTangentsAndBitangents())
        {
            v.tangent[0] = mesh->mTangents[i].x;
            v.tangent[1] = mesh->mTangents[i].y;
            v.tangent[2] = mesh->mTangents[i].z;
            v.tangent[3] = 1.0f;
        }
    }

    outSub.indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
    for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
            outSub.indices.push_back(face.mIndices[j]);
    }
}

void ModelLoader::ExtractMaterials(const void* aiScenePtr,
                                   const std::string& modelDir,
                                   CookedModel& outModel)
{
    const aiScene* scene = static_cast<const aiScene*>(aiScenePtr);
    outModel.materials.resize(scene->mNumMaterials);

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
    {
        aiMaterial* aiMat = scene->mMaterials[i];
        CookedMaterial& cm = outModel.materials[i];

        auto getTex = [&](aiTextureType type, std::string& outPath) {
            if (aiMat->GetTextureCount(type) > 0)
            {
                aiString aiPath;
                aiMat->GetTexture(type, 0, &aiPath);
                outPath = (std::filesystem::path(modelDir) / aiPath.C_Str()).string();
            }
        };

        getTex(aiTextureType_DIFFUSE,  cm.albedoTexturePath);
        getTex(aiTextureType_NORMALS,  cm.normalTexturePath);
        getTex(aiTextureType_EMISSIVE, cm.emissiveTexturePath);

        aiColor4D color;
        if (AI_SUCCESS == aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color))
        {
            cm.baseColorFactor[0] = color.r; cm.baseColorFactor[1] = color.g;
            cm.baseColorFactor[2] = color.b; cm.baseColorFactor[3] = color.a;
        }
    }
}

} // namespace lacrima::renderer

