#include "renderer/vulkan/VulkanSwapchain.h"

#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include "renderer/vulkan/VulkanContext.h"

#include <algorithm>
#include <limits>

namespace dt::renderer
{
    VulkanSwapchain::~VulkanSwapchain()
    {
        // Shutdown must be called explicitly by VulkanRenderer before destruction
        // so the VulkanContext reference is still valid.
        DT_ASSERT(m_swapchain == VK_NULL_HANDLE,
            "VulkanSwapchain destroyed without calling Shutdown() - resource leak");
    }

    bool VulkanSwapchain::Initialize(VulkanContext& ctx,
                                      VkSurfaceKHR   surface,
                                      VkRenderPass   renderPass,
                                      u32            initialWidth,
                                      u32            initialHeight)
    {
        return Recreate(ctx, surface, renderPass, initialWidth, initialHeight);
    }

    void VulkanSwapchain::Shutdown(VulkanContext& ctx)
    {
        if (ctx.Device() == VK_NULL_HANDLE)
            return;

        DestroyFramebuffers(ctx);
        DestroyImageViews(ctx);

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(ctx.Device(), m_swapchain, nullptr);
            m_swapchain = VK_NULL_HANDLE;
        }
    }

    bool VulkanSwapchain::Recreate(VulkanContext& ctx,
                                    VkSurfaceKHR   surface,
                                    VkRenderPass   renderPass,
                                    u32            newWidth,
                                    u32            newHeight)
    {
        VkDevice device = ctx.Device();

        // Query surface capabilities, formats, and present modes
        VkSurfaceCapabilitiesKHR caps{};
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(ctx.PhysicalDevice(), surface, &caps);

        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.PhysicalDevice(), surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(ctx.PhysicalDevice(), surface, &formatCount, formats.data());

        u32 presentCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.PhysicalDevice(), surface, &presentCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(ctx.PhysicalDevice(), surface, &presentCount, presentModes.data());

        if (formats.empty() || presentModes.empty())
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanSwapchain: no valid surface formats or present modes");
            return false;
        }

        VkSurfaceFormatKHR surfaceFormat = SelectFormat(formats);
        VkPresentModeKHR   presentMode   = SelectPresentMode(presentModes);
        VkExtent2D         extent        = SelectExtent(caps, newWidth, newHeight);

        // Request one more image than minimum to avoid waiting on driver
        u32 imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
            imageCount = caps.maxImageCount;

        VkSwapchainKHR oldSwapchain = m_swapchain;

        VkSwapchainCreateInfoKHR ci{};
        ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface          = surface;
        ci.minImageCount    = imageCount;
        ci.imageFormat      = surfaceFormat.format;
        ci.imageColorSpace  = surfaceFormat.colorSpace;
        ci.imageExtent      = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        u32 queueFamilies[] = {
            ctx.GraphicsQueueFamilyIndex(),
            ctx.PresentQueueFamilyIndex()
        };

        if (ctx.GraphicsQueueFamilyIndex() != ctx.PresentQueueFamilyIndex())
        {
            ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices   = queueFamilies;
        }
        else
        {
            ci.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            ci.queueFamilyIndexCount = 0;
            ci.pQueueFamilyIndices   = nullptr;
        }

        ci.preTransform   = caps.currentTransform;
        ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode    = presentMode;
        ci.clipped        = VK_TRUE;
        ci.oldSwapchain   = oldSwapchain;

        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        if (vkCreateSwapchainKHR(device, &ci, nullptr, &newSwapchain) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanSwapchain: vkCreateSwapchainKHR failed");
            return false;
        }

        // Destroy old resources before reassigning
        DestroyFramebuffers(ctx);
        DestroyImageViews(ctx);
        if (oldSwapchain != VK_NULL_HANDLE)
            vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

        m_swapchain   = newSwapchain;
        m_imageFormat = surfaceFormat.format;
        m_extent      = extent;

        // Retrieve swapchain images
        u32 actualCount = 0;
        vkGetSwapchainImagesKHR(device, m_swapchain, &actualCount, nullptr);
        m_images.resize(actualCount);
        vkGetSwapchainImagesKHR(device, m_swapchain, &actualCount, m_images.data());

        // Create image views
        m_imageViews.resize(actualCount);
        for (u32 i = 0; i < actualCount; ++i)
        {
            VkImageViewCreateInfo viewCI{};
            viewCI.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCI.image                           = m_images[i];
            viewCI.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewCI.format                          = m_imageFormat;
            viewCI.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCI.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCI.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCI.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCI.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCI.subresourceRange.baseMipLevel   = 0;
            viewCI.subresourceRange.levelCount     = 1;
            viewCI.subresourceRange.baseArrayLayer = 0;
            viewCI.subresourceRange.layerCount     = 1;

            if (vkCreateImageView(device, &viewCI, nullptr, &m_imageViews[i]) != VK_SUCCESS)
            {
                DT_LOG_ERROR(LogCategory::Renderer, "VulkanSwapchain: failed to create image view {}", i);
                return false;
            }
        }

        // Create framebuffers (one per swapchain image, referencing the render pass)
        m_framebuffers.resize(actualCount);
        for (u32 i = 0; i < actualCount; ++i)
        {
            VkImageView attachments[] = { m_imageViews[i] };

            VkFramebufferCreateInfo fbCI{};
            fbCI.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbCI.renderPass      = renderPass;
            fbCI.attachmentCount = 1;
            fbCI.pAttachments    = attachments;
            fbCI.width           = m_extent.width;
            fbCI.height          = m_extent.height;
            fbCI.layers          = 1;

            if (vkCreateFramebuffer(device, &fbCI, nullptr, &m_framebuffers[i]) != VK_SUCCESS)
            {
                DT_LOG_ERROR(LogCategory::Renderer, "VulkanSwapchain: failed to create framebuffer {}", i);
                return false;
            }
        }

        DT_LOG_INFO(LogCategory::Renderer,
            "VulkanSwapchain: {}x{} with {} images (format={})",
            m_extent.width, m_extent.height, actualCount,
            static_cast<int>(m_imageFormat));
        return true;
    }

    void VulkanSwapchain::DestroyImageViews(VulkanContext& ctx)
    {
        for (auto view : m_imageViews)
        {
            if (view != VK_NULL_HANDLE)
                vkDestroyImageView(ctx.Device(), view, nullptr);
        }
        m_imageViews.clear();
        m_images.clear();
    }

    void VulkanSwapchain::DestroyFramebuffers(VulkanContext& ctx)
    {
        for (auto fb : m_framebuffers)
        {
            if (fb != VK_NULL_HANDLE)
                vkDestroyFramebuffer(ctx.Device(), fb, nullptr);
        }
        m_framebuffers.clear();
    }

    VkSurfaceFormatKHR VulkanSwapchain::SelectFormat(
        const std::vector<VkSurfaceFormatKHR>& available) const
    {
        for (const auto& fmt : available)
        {
            if (fmt.format     == VK_FORMAT_B8G8R8A8_SRGB &&
                fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return fmt;
            }
        }
        return available[0]; // fallback: first available format
    }

    VkPresentModeKHR VulkanSwapchain::SelectPresentMode(
        const std::vector<VkPresentModeKHR>& available) const
    {
        for (auto mode : available)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
                return mode; // triple-buffered, lowest latency
        }
        return VK_PRESENT_MODE_FIFO_KHR; // always available; v-sync
    }

    VkExtent2D VulkanSwapchain::SelectExtent(const VkSurfaceCapabilitiesKHR& caps,
                                              u32 requestedWidth, u32 requestedHeight) const
    {
        if (caps.currentExtent.width != std::numeric_limits<u32>::max())
            return caps.currentExtent;

        VkExtent2D extent{};
        extent.width  = std::clamp(requestedWidth,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(requestedHeight, caps.minImageExtent.height, caps.maxImageExtent.height);
        return extent;
    }
}
