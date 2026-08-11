#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

#include <vector>

// ---------------------------------------------------------------------------
// VulkanSync.h
//
// Manages the synchronization primitives required for rendering frames:
// Semaphores and Fences.
//
// We need three types of sync objects per frame-in-flight:
//   1. imageAvailableSemaphore - signaled by vkAcquireNextImageKHR when
//      an image is ready for writing. Graphics queue waits on this.
//   2. renderFinishedSemaphore - signaled by the graphics queue submission
//      when rendering is complete. Present queue waits on this.
//   3. inFlightFence - signaled when the command buffer finishes executing
//      on the GPU. CPU waits on this before starting to record the next frame.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;

    class VulkanSync
    {
    public:
        VulkanSync() = default;
        ~VulkanSync();

        VulkanSync(const VulkanSync&)            = delete;
        VulkanSync& operator=(const VulkanSync&) = delete;
        VulkanSync(VulkanSync&&)                 = delete;
        VulkanSync& operator=(VulkanSync&&)      = delete;

        bool Initialize(VulkanContext& ctx, u32 frameCount);
        void Shutdown(VulkanContext& ctx);

        VkSemaphore ImageAvailableSemaphore(u32 index) const { return m_imageAvailableSemaphores[index]; }
        VkSemaphore RenderFinishedSemaphore(u32 index) const { return m_renderFinishedSemaphores[index]; }
        VkFence     InFlightFence(u32 index)           const { return m_inFlightFences[index]; }

        bool IsInitialized() const { return !m_inFlightFences.empty(); }

    private:
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence>     m_inFlightFences;
    };
}
