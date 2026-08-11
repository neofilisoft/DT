#include "renderer/vulkan/VulkanBuffer.h"

#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include "renderer/vulkan/VulkanContext.h"

#include <cstring>

namespace dt::renderer
{
    VulkanBuffer::~VulkanBuffer()
    {
        DT_ASSERT(m_buffer == VK_NULL_HANDLE,
            "VulkanBuffer destroyed without calling Shutdown() - resource leak");
    }

    bool VulkanBuffer::Initialize(VulkanContext& ctx,
                                  VkDeviceSize   size,
                                  VkBufferUsageFlags usage,
                                  VkMemoryPropertyFlags properties)
    {
        VkDevice device = ctx.Device();
        m_size = size;

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size        = size;
        bufferInfo.usage       = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanBuffer: failed to create VkBuffer of size {}", size);
            return false;
        }

        VkMemoryRequirements memRequirements{};
        vkGetBufferMemoryRequirements(device, m_buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = ctx.FindMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_memory) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanBuffer: failed to allocate device memory");
            vkDestroyBuffer(device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
            return false;
        }

        vkBindBufferMemory(device, m_buffer, m_memory, 0);

        // Lazily map the buffer if it is host-visible.
        if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        {
            Map(ctx);
        }

        return true;
    }

    void VulkanBuffer::Shutdown(VulkanContext& ctx)
    {
        VkDevice device = ctx.Device();
        if (device == VK_NULL_HANDLE)
            return;

        if (m_mapped != nullptr)
        {
            Unmap(ctx);
        }

        if (m_buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
        }

        if (m_memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, m_memory, nullptr);
            m_memory = VK_NULL_HANDLE;
        }
    }

    void* VulkanBuffer::Map(VulkanContext& ctx)
    {
        if (m_mapped != nullptr)
            return m_mapped;

        VkDevice device = ctx.Device();
        if (vkMapMemory(device, m_memory, 0, m_size, 0, &m_mapped) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanBuffer: failed to map device memory");
            m_mapped = nullptr;
        }
        return m_mapped;
    }

    void VulkanBuffer::Unmap(VulkanContext& ctx)
    {
        if (m_mapped == nullptr)
            return;

        vkUnmapMemory(ctx.Device(), m_memory);
        m_mapped = nullptr;
    }

    void VulkanBuffer::Flush(VulkanContext& ctx, VkDeviceSize offset, VkDeviceSize size)
    {
        VkMappedMemoryRange range{};
        range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = m_memory;
        range.offset = offset;
        range.size   = size;

        vkFlushMappedMemoryRanges(ctx.Device(), 1, &range);
    }

    void VulkanBuffer::CopyData(VulkanContext& ctx, const void* data, VkDeviceSize size)
    {
        DT_ASSERT(m_mapped != nullptr,
            "VulkanBuffer::CopyData called on unmapped or non-host-visible buffer");
        DT_ASSERT(size <= m_size,
            "VulkanBuffer::CopyData overflow: size exceeds buffer size");

        std::memcpy(m_mapped, data, size);
    }
}
