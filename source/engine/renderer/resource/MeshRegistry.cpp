// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/resource/MeshRegistry.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "core/logging/Logger.h"
#include <cstring>

namespace lacrima::renderer
{

// ---------------------------------------------------------------------------
// Internal helper: upload raw vertex + index data into a new GPUMesh
// ---------------------------------------------------------------------------
static bool UploadSubMesh(VulkanContext& ctx,
                          const VulkanMemoryAllocator& allocator,
                          const std::vector<Vertex>& vertices,
                          const std::vector<uint32_t>& indices,
                          GPUMesh& outMesh)
{
    VkDeviceSize vSize = vertices.size() * sizeof(Vertex);
    VkDeviceSize iSize = indices.size()  * sizeof(uint32_t);

    if (!outMesh.Initialize(ctx, allocator, vSize, iSize))
        return false;

    // Vertex staging
    VulkanBuffer staging;
    if (!staging.Initialize(ctx, vSize,
                            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        outMesh.Shutdown(ctx, allocator);
        return false;
    }
    staging.CopyData(ctx, vertices.data(), vSize);

    VkCommandBuffer cmd = ctx.BeginOneTimeCommands();
    VkBufferCopy region{};
    region.size = vSize;
    vkCmdCopyBuffer(cmd, staging.Handle(), outMesh.GetVertexBuffer(), 1, &region);
    ctx.EndOneTimeCommands(cmd);
    staging.Shutdown(ctx);

    // Index staging
    if (iSize > 0)
    {
        VulkanBuffer iStaging;
        if (!iStaging.Initialize(ctx, iSize,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            outMesh.Shutdown(ctx, allocator);
            return false;
        }
        iStaging.CopyData(ctx, indices.data(), iSize);

        cmd = ctx.BeginOneTimeCommands();
        region.size = iSize;
        vkCmdCopyBuffer(cmd, iStaging.Handle(), outMesh.GetIndexBuffer(), 1, &region);
        ctx.EndOneTimeCommands(cmd);
        iStaging.Shutdown(ctx);
    }

    return true;
}

// ---------------------------------------------------------------------------
// MeshRegistry::Upload
// ---------------------------------------------------------------------------
MeshHandle MeshRegistry::Upload(VulkanContext& ctx,
                                const VulkanMemoryAllocator& allocator,
                                const CookedModel& cookedModel)
{
    if (cookedModel.subMeshes.empty())
    {
        LACRIMA_LOG_WARN(lacrima::LogCategory::Renderer, "[MeshRegistry] CookedModel has no sub-meshes, returning INVALID_MESH_HANDLE.");
        return INVALID_MESH_HANDLE;
    }

    MeshHandle handle = m_nextHandle++;
    GPUModel& gpuModel = m_models[handle];
    gpuModel.sourceFilePath = cookedModel.sourceFilePath;
    gpuModel.subMeshes.reserve(cookedModel.subMeshes.size());

    for (const auto& sub : cookedModel.subMeshes)
    {
        GPUSubMesh gpuSub;
        gpuSub.name          = sub.name;
        gpuSub.materialIndex = sub.materialIndex;
        gpuSub.gpuMesh       = std::make_unique<GPUMesh>();

        bool ok = UploadSubMesh(ctx, allocator, sub.vertices, sub.indices, *gpuSub.gpuMesh);
        if (!ok)
        {
            LACRIMA_LOG_ERROR(lacrima::LogCategory::Renderer, "[MeshRegistry] Failed to upload sub-mesh {}", sub.name);
            // Rollback what we uploaded so far
            UnloadModel(ctx, allocator, handle);
            return INVALID_MESH_HANDLE;
        }

        gpuModel.subMeshes.push_back(std::move(gpuSub));
    }

    LACRIMA_LOG_INFO(lacrima::LogCategory::Renderer, "[MeshRegistry] Uploaded model handle={}, subMeshes={}", handle, gpuModel.subMeshes.size());
    return handle;
}

// ---------------------------------------------------------------------------
// MeshRegistry::Get
// ---------------------------------------------------------------------------
const GPUModel* MeshRegistry::Get(MeshHandle handle) const
{
    auto it = m_models.find(handle);
    if (it == m_models.end()) return nullptr;
    return &it->second;
}

// ---------------------------------------------------------------------------
// MeshRegistry::UnloadModel
// ---------------------------------------------------------------------------
void MeshRegistry::UnloadModel(VulkanContext& ctx,
                               const VulkanMemoryAllocator& allocator,
                               MeshHandle handle)
{
    auto it = m_models.find(handle);
    if (it == m_models.end()) return;

    for (auto& sub : it->second.subMeshes)
    {
        if (sub.gpuMesh)
            sub.gpuMesh->Shutdown(ctx, allocator);
    }
    m_models.erase(it);
}

// ---------------------------------------------------------------------------
// MeshRegistry::Shutdown
// ---------------------------------------------------------------------------
void MeshRegistry::Shutdown(VulkanContext& ctx, const VulkanMemoryAllocator& allocator)
{
    for (auto& [handle, model] : m_models)
    {
        for (auto& sub : model.subMeshes)
        {
            if (sub.gpuMesh)
                sub.gpuMesh->Shutdown(ctx, allocator);
        }
    }
    m_models.clear();
    m_nextHandle = 1;
}

} // namespace lacrima::renderer

