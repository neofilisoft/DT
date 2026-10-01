#pragma once

#include "core/platform/Types.h"
#include "renderer/vulkan/VulkanContext.h"

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace lacrima::renderer
{
    // High-performance, zero-leak offscreen render target for the 3D scene Viewport.
    // Encapsulates dedicated color and depth images, custom render pass with automatic
    // transition to SHADER_READ_ONLY_OPTIMAL, and registered ImGui texture descriptor.
    class VulkanOffscreenTarget
    {
    public:
        VulkanOffscreenTarget() = default;
        ~VulkanOffscreenTarget();

        VulkanOffscreenTarget(const VulkanOffscreenTarget&) = delete;
        VulkanOffscreenTarget& operator=(const VulkanOffscreenTarget&) = delete;
        VulkanOffscreenTarget(VulkanOffscreenTarget&&) = delete;
        VulkanOffscreenTarget& operator=(VulkanOffscreenTarget&&) = delete;

        bool Initialize(VulkanContext& ctx, u32 width, u32 height, VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM);
        void Shutdown(VulkanContext& ctx);
        bool Resize(VulkanContext& ctx, u32 newWidth, u32 newHeight);

        VkRenderPass Handle() const { return m_renderPass; }
        VkFramebuffer Framebuffer() const { return m_framebuffer; }
        VkDescriptorSet DescriptorSet() const { return m_descriptorSet; }
        VkExtent2D Extent() const { return { m_width, m_height }; }
        u32 Width() const { return m_width; }
        u32 Height() const { return m_height; }
        bool IsInitialized() const { return m_framebuffer != VK_NULL_HANDLE; }

    private:
        bool CreateRenderPass(VulkanContext& ctx);
        bool CreateColorResources(VulkanContext& ctx);
        bool CreateDepthResources(VulkanContext& ctx);
        bool CreateSampler(VulkanContext& ctx);
        bool CreateFramebuffer(VulkanContext& ctx);
        bool RegisterImGuiDescriptor();

        void DestroyTargetResources(VulkanContext& ctx);

        u32 m_width = 0;
        u32 m_height = 0;

        VkFormat m_colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
        VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT;

        VkRenderPass m_renderPass = VK_NULL_HANDLE;
        VkFramebuffer m_framebuffer = VK_NULL_HANDLE;

        VkImage m_colorImage = VK_NULL_HANDLE;
        VkDeviceMemory m_colorMemory = VK_NULL_HANDLE;
        VkImageView m_colorView = VK_NULL_HANDLE;

        VkImage m_depthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_depthMemory = VK_NULL_HANDLE;
        VkImageView m_depthView = VK_NULL_HANDLE;

        VkSampler m_sampler = VK_NULL_HANDLE;
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    };
}
