#include "renderer/vulkan/VulkanMemoryAllocator.h"
#include "renderer/vulkan/VulkanContext.h"
#include "core/logging/Logger.h"

namespace dt::renderer
{
    bool VulkanMemoryAllocator::Initialize(const VulkanContext* context)
    {
        m_context = context;
        return true;
    }

    void VulkanMemoryAllocator::Shutdown(const VulkanContext* context)
    {
        m_context = nullptr;
    }

    VkDeviceMemory VulkanMemoryAllocator::AllocateBufferMemory(VkBuffer buffer, VkMemoryPropertyFlags properties) const
    {
        if (!m_context) return VK_NULL_HANDLE;

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_context->Device(), buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        // Find appropriate memory type
        bool found = false;
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_context->PhysicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((memRequirements.memoryTypeBits & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                allocInfo.memoryTypeIndex = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanMemoryAllocator: failed to find suitable memory type for buffer");
            return VK_NULL_HANDLE;
        }

        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (vkAllocateMemory(m_context->Device(), &allocInfo, nullptr, &memory) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanMemoryAllocator: failed to allocate buffer memory");
            return VK_NULL_HANDLE;
        }

        vkBindBufferMemory(m_context->Device(), buffer, memory, 0);
        return memory;
    }

    VkDeviceMemory VulkanMemoryAllocator::AllocateImageMemory(VkImage image, VkMemoryPropertyFlags properties) const
    {
        if (!m_context) return VK_NULL_HANDLE;

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_context->Device(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;

        // Find appropriate memory type
        bool found = false;
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_context->PhysicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((memRequirements.memoryTypeBits & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                allocInfo.memoryTypeIndex = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanMemoryAllocator: failed to find suitable memory type for image");
            return VK_NULL_HANDLE;
        }

        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (vkAllocateMemory(m_context->Device(), &allocInfo, nullptr, &memory) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanMemoryAllocator: failed to allocate image memory");
            return VK_NULL_HANDLE;
        }

        vkBindImageMemory(m_context->Device(), image, memory, 0);
        return memory;
    }

    void VulkanMemoryAllocator::FreeMemory(VkDeviceMemory memory) const
    {
        if (m_context && memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_context->Device(), memory, nullptr);
        }
    }
}
