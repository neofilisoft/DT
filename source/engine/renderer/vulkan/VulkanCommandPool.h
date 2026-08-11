#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

#include <vector>

// ---------------------------------------------------------------------------
// VulkanCommandPool.h
//
// Manages the graphics command pool and per-frame command buffers.
//
// In Vulkan, command buffers are allocated from a command pool.
// For double/triple buffering, we allocate multiple command buffers (one per
// swapchain image / frame in flight) from the same pool so they can be
// recorded and submitted concurrently.
//
// Flags:
//   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT is used to allow resetting
//   individual command buffers before recording each frame.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;

    class VulkanCommandPool
    {
    public:
        VulkanCommandPool() = default;
        ~VulkanCommandPool();

        VulkanCommandPool(const VulkanCommandPool&)            = delete;
        VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;
        VulkanCommandPool(VulkanCommandPool&&)                 = delete;
        VulkanCommandPool& operator=(VulkanCommandPool&&)      = delete;

        bool Initialize(VulkanContext& ctx, u32 bufferCount);
        void Shutdown(VulkanContext& ctx);

        VkCommandPool Handle() const { return m_pool; }
        VkCommandBuffer Buffer(u32 index) const { return m_buffers[index]; }

        bool IsInitialized() const { return m_pool != VK_NULL_HANDLE; }

    private:
        VkCommandPool                m_pool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_buffers;
    };
}
