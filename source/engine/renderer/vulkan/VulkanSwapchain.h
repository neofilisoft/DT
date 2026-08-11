#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

#include <vector>

// ---------------------------------------------------------------------------
// VulkanSwapchain.h
//
// Owns the VkSwapchainKHR and all per-image resources derived from it:
// VkImageView[] and VkFramebuffer[]. The framebuffers reference a
// VkRenderPass owned by VulkanRenderPass - the swapchain holds a non-owning
// reference to that render pass handle.
//
// Swapchain recreation: called when the window is resized or the swapchain
// returns VK_ERROR_OUT_OF_DATE_KHR / VK_SUBOPTIMAL_KHR. Recreate() destroys
// the old swapchain (and its image views/framebuffers) and creates a new
// one, reusing the same VkSurfaceKHR. The caller (VulkanRenderer) must
// ensure no in-flight frames are using the old swapchain before calling
// Recreate() (done via vkDeviceWaitIdle in the renderer's resize path).
//
// Format and present mode selection:
//   Format:       VK_FORMAT_B8G8R8A8_SRGB with VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
//                 falling back to the first available format.
//   Present mode: VK_PRESENT_MODE_MAILBOX_KHR (triple-buffered non-blocking)
//                 falling back to VK_PRESENT_MODE_FIFO_KHR (always available).
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;

    class VulkanSwapchain
    {
    public:
        VulkanSwapchain() = default;
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain&)            = delete;
        VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
        VulkanSwapchain(VulkanSwapchain&&)                 = delete;
        VulkanSwapchain& operator=(VulkanSwapchain&&)      = delete;

        // Returns false if swapchain creation fails (e.g. surface lost).
        bool Initialize(VulkanContext& ctx,
                        VkSurfaceKHR   surface,
                        VkRenderPass   renderPass,
                        u32            initialWidth,
                        u32            initialHeight);

        void Shutdown(VulkanContext& ctx);

        // Destroy old swapchain and create a new one at the current surface
        // dimensions. renderPass must be the same compatible render pass.
        bool Recreate(VulkanContext& ctx,
                      VkSurfaceKHR   surface,
                      VkRenderPass   renderPass,
                      u32            newWidth,
                      u32            newHeight);

        // --- Per-frame accessors -------------------------------------------

        VkSwapchainKHR       Handle()            const { return m_swapchain; }
        VkFormat             ImageFormat()        const { return m_imageFormat; }
        VkExtent2D           Extent()             const { return m_extent; }
        u32                  ImageCount()         const { return static_cast<u32>(m_images.size()); }
        VkFramebuffer        Framebuffer(u32 i)   const { return m_framebuffers[i]; }

    private:
        void DestroyImageViews(VulkanContext& ctx);
        void DestroyFramebuffers(VulkanContext& ctx);

        VkSurfaceFormatKHR SelectFormat(const std::vector<VkSurfaceFormatKHR>& available) const;
        VkPresentModeKHR   SelectPresentMode(const std::vector<VkPresentModeKHR>& available) const;
        VkExtent2D         SelectExtent(const VkSurfaceCapabilitiesKHR& caps,
                                        u32 requestedWidth, u32 requestedHeight) const;

        VkSwapchainKHR           m_swapchain    = VK_NULL_HANDLE;
        VkFormat                 m_imageFormat  = VK_FORMAT_UNDEFINED;
        VkExtent2D               m_extent       = {};
        std::vector<VkImage>     m_images;
        std::vector<VkImageView> m_imageViews;
        std::vector<VkFramebuffer> m_framebuffers;
    };
}
