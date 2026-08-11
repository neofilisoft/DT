#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
// VulkanMemoryAllocator.h
//
// A simple RAII wrapper for Vulkan memory allocation.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;

    class VulkanMemoryAllocator
    {
    public:
        VulkanMemoryAllocator() = default;
        ~VulkanMemoryAllocator() = default;

        // Non-copyable, non-movable
        VulkanMemoryAllocator(const VulkanMemoryAllocator&) = delete;
        VulkanMemoryAllocator& operator=(const VulkanMemoryAllocator&) = delete;
        VulkanMemoryAllocator(VulkanMemoryAllocator&&) = delete;
        VulkanMemoryAllocator& operator=(VulkanMemoryAllocator&&) = delete;

        bool Initialize(const VulkanContext* context);
        void Shutdown(const VulkanContext* context);

        // Allocates memory for a buffer and binds it
        VkDeviceMemory AllocateBufferMemory(VkBuffer buffer, VkMemoryPropertyFlags properties) const;

        // Allocates memory for an image and binds it
        VkDeviceMemory AllocateImageMemory(VkImage image, VkMemoryPropertyFlags properties) const;

        // Frees memory
        void FreeMemory(VkDeviceMemory memory) const;

    private:
        const VulkanContext* m_context = nullptr;
    };
}
