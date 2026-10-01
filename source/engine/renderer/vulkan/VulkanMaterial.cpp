// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/vulkan/VulkanMaterial.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanDescriptorPool.h"
#include "renderer/resource/GPUTexture.h"
#include "core/logging/Logger.h"

namespace lacrima::renderer
{
    bool VulkanMaterial::CreateDescriptorSetLayout(VulkanContext& ctx, VkDescriptorSetLayout& outLayout)
    {
        VkDescriptorSetLayoutBinding samplerLayoutBindings[5]{};
        for (int i = 0; i < 5; ++i) {
            samplerLayoutBindings[i].binding = i;
            samplerLayoutBindings[i].descriptorCount = 1;
            samplerLayoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            samplerLayoutBindings[i].pImmutableSamplers = nullptr;
            samplerLayoutBindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 5;
        layoutInfo.pBindings = samplerLayoutBindings;

        if (vkCreateDescriptorSetLayout(ctx.Device(), &layoutInfo, nullptr, &outLayout) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanMaterial: failed to create descriptor set layout");
            return false;
        }

        return true;
    }

    bool VulkanMaterial::Initialize(VulkanContext& ctx, 
                                    VulkanDescriptorPool& pool, 
                                    VkDescriptorSetLayout layout, 
                                    const GPUTexture& albedo,
                                    const GPUTexture& normal,
                                    const GPUTexture& metallicRoughness,
                                    const GPUTexture& ao,
                                    const GPUTexture& emissive)
    {
        if (!pool.AllocateDescriptorSet(ctx, layout, m_descriptorSet))
        {
            return false;
        }

        const GPUTexture* textures[5] = { &albedo, &normal, &metallicRoughness, &ao, &emissive };
        VkDescriptorImageInfo imageInfos[5]{};
        VkWriteDescriptorSet descriptorWrites[5]{};

        for (int i = 0; i < 5; ++i) {
            imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos[i].imageView = textures[i]->GetImageView();
            imageInfos[i].sampler = textures[i]->GetSampler();

            descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[i].dstSet = m_descriptorSet;
            descriptorWrites[i].dstBinding = i;
            descriptorWrites[i].dstArrayElement = 0;
            descriptorWrites[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[i].descriptorCount = 1;
            descriptorWrites[i].pImageInfo = &imageInfos[i];
        }

        vkUpdateDescriptorSets(ctx.Device(), 5, descriptorWrites, 0, nullptr);

        return true;
    }
}
