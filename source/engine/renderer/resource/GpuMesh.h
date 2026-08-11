#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// GpuMesh.h
//
// Represents a Vulkan mesh containing vertex and index buffers.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;
    class VulkanMemoryAllocator;

    struct Vertex
    {
        float position[3];
        float normal[3];
        float texCoord[2];
    };

    class GpuMesh
    {
    public:
        GpuMesh() = default;
        ~GpuMesh();

        GpuMesh(const GpuMesh&) = delete;
        GpuMesh& operator=(const GpuMesh&) = delete;
        GpuMesh(GpuMesh&&) = delete;
        GpuMesh& operator=(GpuMesh&&) = delete;

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
