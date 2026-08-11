#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>
#include <vector>

namespace dt::renderer
{
    class VulkanContext;

    class VulkanDescriptorPool
    {
    public:
        VulkanDescriptorPool() = default;
        ~VulkanDescriptorPool();

        bool Initialize(VulkanContext& ctx, u32 maxSets, u32 maxUniformBuffers, u32 maxImageSamplers);
        void Shutdown(VulkanContext& ctx);

        // Allocates a descriptor set based on a provided layout
        bool AllocateDescriptorSet(VulkanContext& ctx, VkDescriptorSetLayout layout, VkDescriptorSet& outSet);
        
        // Note: For simplicity in M7, we won't implement freeing individual sets,
        // we would normally Reset() the entire pool or just keep it around.
        void Reset(VulkanContext& ctx);

        VkDescriptorPool Handle() const { return m_pool; }

    private:
        VkDescriptorPool m_pool = VK_NULL_HANDLE;
    };
}
