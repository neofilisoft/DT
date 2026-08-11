#include "renderer/resource/GpuMesh.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMemoryAllocator.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include <fstream>

namespace dt::renderer
{
    GpuMesh::~GpuMesh()
    {
        DT_ASSERT(m_vertexBuffer == VK_NULL_HANDLE && m_indexBuffer == VK_NULL_HANDLE, 
                  "GpuMesh destroyed without calling Shutdown()");
    }

    bool GpuMesh::Initialize(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                             VkDeviceSize vertexBufferSize, VkDeviceSize indexBufferSize)
    {
        // Vertex buffer
        VkBufferCreateInfo vertexInfo{};
        vertexInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexInfo.size = vertexBufferSize;
        vertexInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        vertexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(ctx.Device(), &vertexInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuMesh: failed to create vertex buffer");
            return false;
        }

        m_vertexMemory = allocator.AllocateBufferMemory(m_vertexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (m_vertexMemory == VK_NULL_HANDLE)
        {
            return false;
        }

        // Index buffer
        if (indexBufferSize > 0)
        {
            VkBufferCreateInfo indexInfo{};
            indexInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            indexInfo.size = indexBufferSize;
            indexInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            indexInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(ctx.Device(), &indexInfo, nullptr, &m_indexBuffer) != VK_SUCCESS)
            {
                DT_LOG_ERROR(LogCategory::Renderer, "GpuMesh: failed to create index buffer");
                return false;
            }

            m_indexMemory = allocator.AllocateBufferMemory(m_indexBuffer, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (m_indexMemory == VK_NULL_HANDLE)
            {
                return false;
            }
        }

        return true;
    }

    void GpuMesh::Shutdown(VulkanContext& ctx, const VulkanMemoryAllocator& allocator)
    {
        if (m_indexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(ctx.Device(), m_indexBuffer, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
        }

        if (m_indexMemory != VK_NULL_HANDLE)
        {
            allocator.FreeMemory(m_indexMemory);
            m_indexMemory = VK_NULL_HANDLE;
        }

        if (m_vertexBuffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(ctx.Device(), m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }

        if (m_vertexMemory != VK_NULL_HANDLE)
        {
            allocator.FreeMemory(m_vertexMemory);
            m_vertexMemory = VK_NULL_HANDLE;
        }
    }

    bool GpuMesh::LoadFromCookedFile(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                                     const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuMesh: failed to open file '{}'", path);
            return false;
        }

        struct AssetHeader {
            char magic[4];
            u32 version;
            u32 type;
        } header;

        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic[0] != 'D' || header.magic[1] != 'T' || header.magic[2] != 'A' || header.magic[3] != 'S')
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuMesh: invalid magic in file '{}'", path);
            return false;
        }
        if (header.type != 2)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuMesh: asset type is not mesh in file '{}'", path);
            return false;
        }

        struct MeshPayloadHeader {
            u32 vertexCount;
            u32 indexCount;
        } meshHeader;

        file.read(reinterpret_cast<char*>(&meshHeader), sizeof(meshHeader));

        VkDeviceSize vertexSize = meshHeader.vertexCount * sizeof(Vertex);
        VkDeviceSize indexSize = meshHeader.indexCount * sizeof(uint32_t);

        std::vector<Vertex> vertices(meshHeader.vertexCount);
        file.read(reinterpret_cast<char*>(vertices.data()), vertexSize);

        std::vector<uint32_t> indices(meshHeader.indexCount);
        if (indexSize > 0)
        {
            file.read(reinterpret_cast<char*>(indices.data()), indexSize);
        }

        m_indexCount = meshHeader.indexCount;
        
        if (!Initialize(ctx, allocator, meshHeader.vertexCount * sizeof(Vertex), m_indexCount * sizeof(u32)))
        {
            return false;
        }

        VulkanBuffer stagingBuffer;
        if (!stagingBuffer.Initialize(ctx, vertexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Shutdown(ctx, allocator);
            return false;
        }
        stagingBuffer.CopyData(ctx, vertices.data(), vertexSize);

        VkCommandBuffer cmd = ctx.BeginOneTimeCommands();
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = vertexSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.Handle(), m_vertexBuffer, 1, &copyRegion);
        ctx.EndOneTimeCommands(cmd);

        stagingBuffer.Shutdown(ctx);

        if (indexSize > 0)
        {
            if (!stagingBuffer.Initialize(ctx, indexSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            {
                Shutdown(ctx, allocator);
                return false;
            }
            stagingBuffer.CopyData(ctx, indices.data(), indexSize);

            cmd = ctx.BeginOneTimeCommands();
            copyRegion.size = indexSize;
            vkCmdCopyBuffer(cmd, stagingBuffer.Handle(), m_indexBuffer, 1, &copyRegion);
            ctx.EndOneTimeCommands(cmd);

            stagingBuffer.Shutdown(ctx);
        }

        return true;
    }
}
