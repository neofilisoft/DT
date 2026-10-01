// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// GPUMesh.h
//
// Represents a Vulkan mesh containing vertex and index buffers.
// ---------------------------------------------------------------------------

namespace lacrima::renderer
{
    class VulkanContext;
    class VulkanMemoryAllocator;

    struct Vertex
    {
        float position[3];
        float normal[3];
        float texCoord[2];
        float tangent[4];   // xyz = direction, w = handedness
    };

    /// Skinned mesh vertex - extends Vertex with bone influence data.
    /// Used by SkinnedMeshPass for GPU vertex skinning (Step 3).
    struct SkinnedVertex
    {
        float    position[3];
        float    normal[3];
        float    texCoord[2];
        float    tangent[4];
        uint32_t boneIndices[4]; // up to 4 bone influences per vertex
        float    boneWeights[4]; // must sum to 1.0
    };

    class GPUMesh
    {
    public:
        GPUMesh() = default;
        ~GPUMesh();

        GPUMesh(const GPUMesh&) = delete;
        GPUMesh& operator=(const GPUMesh&) = delete;
        GPUMesh(GPUMesh&&) = delete;
        GPUMesh& operator=(GPUMesh&&) = delete;

        bool Initialize(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                        VkDeviceSize vertexBufferSize, VkDeviceSize indexBufferSize);
        bool LoadFromCookedFile(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                                const std::string& path);
        
        void Shutdown(VulkanContext& ctx, const VulkanMemoryAllocator& allocator);

        VkBuffer GetVertexBuffer() const { return m_vertexBuffer; }
        VkBuffer GetIndexBuffer() const { return m_indexBuffer; }
        u32 GetIndexCount() const { return m_indexCount; }

    private:
        VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vertexMemory = VK_NULL_HANDLE;

        VkBuffer m_indexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_indexMemory = VK_NULL_HANDLE;
        
        u32 m_indexCount = 0;
    };
}


