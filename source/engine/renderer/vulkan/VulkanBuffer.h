#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
// VulkanBuffer.h
//
// RAII wrapper around a VkBuffer and its bound VkDeviceMemory.
// Designed for simple allocations (uniform buffers, vertex buffers, and
// staging buffers) in raw Vulkan without VMA to keep dependencies minimal
// abstraction.
//
// In Vulkan, allocating GPU memory requires querying the buffer's memory
// requirements, finding a matching memory type index from the physical
// device properties, allocating the memory block, and binding it to the
// buffer. This wrapper handles all of that inside Initialize().
//
// Memory mapping: buffers allocated with host-visible properties can be
// mapped directly using Map()/Unmap() to write data from the CPU.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;

    class VulkanBuffer
    {
    public:
        VulkanBuffer() = default;
        ~VulkanBuffer();

        VulkanBuffer(const VulkanBuffer&)            = delete;
        VulkanBuffer& operator=(const VulkanBuffer&) = delete;
        VulkanBuffer(VulkanBuffer&&)                 = delete;
        VulkanBuffer& operator=(VulkanBuffer&&)      = delete;

        bool Initialize(VulkanContext& ctx,
                        VkDeviceSize   size,
                        VkBufferUsageFlags usage,
                        VkMemoryPropertyFlags properties);

        void Shutdown(VulkanContext& ctx);

        VkBuffer     Handle() const { return m_buffer; }
        VkDeviceSize Size()   const { return m_size; }
        void*        Mapped() const { return m_mapped; }

        // Maps the memory to a CPU-accessible pointer if it isn't already mapped.
        // Returns nullptr if mapping fails.
        void* Map(VulkanContext& ctx);
        void  Unmap(VulkanContext& ctx);

        // Flushes host writes to make them visible to the GPU (only needed
        // if the memory properties do not include VK_MEMORY_PROPERTY_HOST_COHERENT_BIT).
        void Flush(VulkanContext& ctx, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);

        // Helper to copy a block of memory from the CPU into this buffer.
        // If the buffer is host-visible, it maps, copies, and unmaps.
        // Asserts if buffer is not host-visible.
        void CopyData(VulkanContext& ctx, const void* data, VkDeviceSize size);

        bool IsInitialized() const { return m_buffer != VK_NULL_HANDLE; }

    private:
        VkBuffer       m_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_memory = VK_NULL_HANDLE;
        VkDeviceSize   m_size   = 0;
        void*          m_mapped = nullptr;
    };
}
