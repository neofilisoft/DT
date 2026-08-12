#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>

namespace dt::renderer
{
    class VulkanContext;
    class VulkanDescriptorPool;
    class GpuTexture;

    // A simple material representation.
    // Binds a single GpuTexture to Descriptor Set 1.
    class VulkanMaterial
    {
    public:
        VulkanMaterial() = default;
        ~VulkanMaterial() = default;

        // Creates a descriptor set layout for a single combined image sampler
        static bool CreateDescriptorSetLayout(VulkanContext& ctx, VkDescriptorSetLayout& outLayout);

        // Initializes the material by allocating a descriptor set and updating it with the texture
        bool Initialize(VulkanContext& ctx, 
                        VulkanDescriptorPool& pool, 
                        VkDescriptorSetLayout layout, 
                        const GpuTexture& texture);

        VkDescriptorSet GetDescriptorSet() const { return m_descriptorSet; }

    private:
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    };
}
