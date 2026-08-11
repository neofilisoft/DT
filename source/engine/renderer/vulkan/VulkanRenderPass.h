#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
// VulkanRenderPass.h
//
// A single VkRenderPass with one color attachment (the swapchain image).
// No depth attachment for M5 (2D quad rendering has no depth test
// requirements). The render pass defines:
//
//   loadOp  = CLEAR   (clear the swapchain image to a background color
//                       at the start of each frame - no need to preserve
//                       contents from the previous frame since we redraw
//                       everything every frame)
//   storeOp = STORE   (write the rendered output to the swapchain image
//                       so it can be presented)
//
// This render pass is compatible with the VulkanSwapchain's framebuffers
// as long as the image format matches (both use the format negotiated
// during swapchain creation). VulkanRenderer ensures this by passing the
// same format to both.
//
// Lifetime: one render pass for the entire application lifetime.
// Swapchain recreation does NOT require recreating the render pass
// as long as the image format stays the same (which it does on a simple
// resize - format only changes if the swapchain is destroyed and the
// user e.g. switches display HDR settings).
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;

    class VulkanRenderPass
    {
    public:
        VulkanRenderPass() = default;
        ~VulkanRenderPass();

        VulkanRenderPass(const VulkanRenderPass&)            = delete;
        VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
        VulkanRenderPass(VulkanRenderPass&&)                 = delete;
        VulkanRenderPass& operator=(VulkanRenderPass&&)      = delete;

        bool Initialize(VulkanContext& ctx, VkFormat colorFormat);
        void Shutdown(VulkanContext& ctx);

        VkRenderPass Handle() const { return m_renderPass; }
        bool IsInitialized() const { return m_renderPass != VK_NULL_HANDLE; }

    private:
        VkRenderPass m_renderPass = VK_NULL_HANDLE;
    };
}
