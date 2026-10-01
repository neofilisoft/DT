// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include <vulkan/vulkan.h>

namespace lacrima::renderer
{
    class VulkanContext;
    class VulkanDescriptorPool;
    class GPUTexture;

    // A simple material representation.
    // Binds up to 5 GPUTextures for a PBR material to Descriptor Set 1.
    class VulkanMaterial
    {
    public:
        VulkanMaterial() = default;
        ~VulkanMaterial() = default;

        // Creates a descriptor set layout for 5 combined image samplers
        static bool CreateDescriptorSetLayout(VulkanContext& ctx, VkDescriptorSetLayout& outLayout);

        // Initializes the material by allocating a descriptor set and updating it with the textures
        bool Initialize(VulkanContext& ctx, 
                        VulkanDescriptorPool& pool, 
                        VkDescriptorSetLayout layout, 
                        const GPUTexture& albedo,
                        const GPUTexture& normal,
                        const GPUTexture& metallicRoughness,
                        const GPUTexture& ao,
                        const GPUTexture& emissive);

        VkDescriptorSet GetDescriptorSet() const { return m_descriptorSet; }
        bool IsInitialized() const { return m_descriptorSet != VK_NULL_HANDLE; }

    private:
        VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    };
}


