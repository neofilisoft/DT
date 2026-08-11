#include "renderer/vulkan/VulkanSync.h"

#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include "renderer/vulkan/VulkanContext.h"

namespace dt::renderer
{
    VulkanSync::~VulkanSync()
    {
        DT_ASSERT(m_inFlightFences.empty(),
            "VulkanSync destroyed without calling Shutdown() - resource leak");
    }

    bool VulkanSync::Initialize(VulkanContext& ctx, u32 frameCount)
    {
        VkDevice device = ctx.Device();

        m_imageAvailableSemaphores.resize(frameCount, VK_NULL_HANDLE);
        m_renderFinishedSemaphores.resize(frameCount, VK_NULL_HANDLE);
        m_inFlightFences.resize(frameCount, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // Start the fence in signaled state so the first wait doesn't hang.
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (u32 i = 0; i < frameCount; ++i)
        {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            {
                DT_LOG_ERROR(LogCategory::Renderer, "VulkanSync: failed to create synchronization primitives for frame {}", i);
                return false;
            }
        }

        return true;
    }

    void VulkanSync::Shutdown(VulkanContext& ctx)
    {
        if (ctx.Device() == VK_NULL_HANDLE)
            return;

        VkDevice device = ctx.Device();

        for (auto sem : m_imageAvailableSemaphores)
        {
            if (sem != VK_NULL_HANDLE)
                vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto sem : m_renderFinishedSemaphores)
        {
            if (sem != VK_NULL_HANDLE)
                vkDestroySemaphore(device, sem, nullptr);
        }
        for (auto fence : m_inFlightFences)
        {
            if (fence != VK_NULL_HANDLE)
                vkDestroyFence(device, fence, nullptr);
        }

        m_imageAvailableSemaphores.clear();
        m_renderFinishedSemaphores.clear();
        m_inFlightFences.clear();
    }
}
