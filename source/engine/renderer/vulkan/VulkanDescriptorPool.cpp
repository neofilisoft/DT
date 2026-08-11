#include "renderer/vulkan/VulkanDescriptorPool.h"
#include "renderer/vulkan/VulkanContext.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"

namespace dt::renderer
{
    VulkanDescriptorPool::~VulkanDescriptorPool()
    {
        DT_ASSERT(m_pool == VK_NULL_HANDLE, "VulkanDescriptorPool destroyed without calling Shutdown()");
    }

    bool VulkanDescriptorPool::Initialize(VulkanContext& ctx, u32 maxSets, u32 maxUniformBuffers, u32 maxImageSamplers)
    {
        std::vector<VkDescriptorPoolSize> poolSizes;
        if (maxUniformBuffers > 0)
        {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxUniformBuffers });
        }
        if (maxImageSamplers > 0)
        {
            poolSizes.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxImageSamplers });
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = maxSets;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT; // Allow freeing individual sets if needed later

        if (vkCreateDescriptorPool(ctx.Device(), &poolInfo, nullptr, &m_pool) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanDescriptorPool: failed to create descriptor pool");
            return false;
        }

        return true;
    }

    void VulkanDescriptorPool::Shutdown(VulkanContext& ctx)
    {
        if (m_pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(ctx.Device(), m_pool, nullptr);
            m_pool = VK_NULL_HANDLE;
        }
    }

    bool VulkanDescriptorPool::AllocateDescriptorSet(VulkanContext& ctx, VkDescriptorSetLayout layout, VkDescriptorSet& outSet)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        if (vkAllocateDescriptorSets(ctx.Device(), &allocInfo, &outSet) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanDescriptorPool: failed to allocate descriptor set");
            return false;
        }

        return true;
    }

    void VulkanDescriptorPool::Reset(VulkanContext& ctx)
    {
        if (m_pool != VK_NULL_HANDLE)
        {
            vkResetDescriptorPool(ctx.Device(), m_pool, 0);
        }
    }
}
