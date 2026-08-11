#include "renderer/vulkan/VulkanCommandPool.h"

#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include "renderer/vulkan/VulkanContext.h"

namespace dt::renderer
{
    VulkanCommandPool::~VulkanCommandPool()
    {
        DT_ASSERT(m_pool == VK_NULL_HANDLE,
            "VulkanCommandPool destroyed without calling Shutdown() - resource leak");
    }

    bool VulkanCommandPool::Initialize(VulkanContext& ctx, u32 bufferCount)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = ctx.GraphicsQueueFamilyIndex();

        if (vkCreateCommandPool(ctx.Device(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanCommandPool: failed to create VkCommandPool");
            return false;
        }

        m_buffers.resize(bufferCount);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = m_pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = bufferCount;

        if (vkAllocateCommandBuffers(ctx.Device(), &allocInfo, m_buffers.data()) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanCommandPool: failed to allocate command buffers");
            return false;
        }

        return true;
    }

    void VulkanCommandPool::Shutdown(VulkanContext& ctx)
    {
        if (ctx.Device() == VK_NULL_HANDLE)
            return;

        if (m_pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(ctx.Device(), m_pool, nullptr);
            m_pool = VK_NULL_HANDLE;
        }
        m_buffers.clear();
    }
}
