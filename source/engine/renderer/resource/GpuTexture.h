#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>
#include <string>

// ---------------------------------------------------------------------------
// GpuTexture.h
//
// Represents a Vulkan image and its associated memory, view, and sampler.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;
    class VulkanMemoryAllocator;

    class GpuTexture
    {
    public:
        GpuTexture() = default;
        ~GpuTexture();

        // Non-copyable, non-movable
        GpuTexture(const GpuTexture&) = delete;
        GpuTexture& operator=(const GpuTexture&) = delete;
        GpuTexture(GpuTexture&&) = delete;
        GpuTexture& operator=(GpuTexture&&) = delete;

        bool Initialize(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                        u32 width, u32 height, VkFormat format);
        bool LoadFromCookedFile(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                                const std::string& path);
        void Shutdown(VulkanContext& ctx, const VulkanMemoryAllocator& allocator);

        VkImage GetImage() const { return m_image; }
        VkImageView GetImageView() const { return m_imageView; }
        VkSampler GetSampler() const { return m_sampler; }

    private:
        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;
        
        u32 m_width = 0;
        u32 m_height = 0;
        VkFormat m_format = VK_FORMAT_UNDEFINED;
    };
}
