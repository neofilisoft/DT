#include "renderer/vulkan/VulkanMaterial.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanDescriptorPool.h"
#include "renderer/resource/GpuTexture.h"
#include "core/logging/Logger.h"

namespace dt::renderer
{
    bool VulkanMaterial::CreateDescriptorSetLayout(VulkanContext& ctx, VkDescriptorSetLayout& outLayout)
    {
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(ctx.Device(), &layoutInfo, nullptr, &outLayout) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanMaterial: failed to create descriptor set layout");
            return false;
        }

        return true;
    }

    bool VulkanMaterial::Initialize(VulkanContext& ctx, 
                                    VulkanDescriptorPool& pool, 
                                    VkDescriptorSetLayout layout, 
                                    const GpuTexture& texture)
    {
        if (!pool.AllocateDescriptorSet(ctx, layout, m_descriptorSet))
        {
            return false;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture.GetImageView();
        imageInfo.sampler = texture.GetSampler();

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(ctx.Device(), 1, &descriptorWrite, 0, nullptr);

        return true;
    }
}
