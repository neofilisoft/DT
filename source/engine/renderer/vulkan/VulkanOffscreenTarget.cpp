#include "renderer/vulkan/VulkanOffscreenTarget.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"

namespace lacrima::renderer
{
    VulkanOffscreenTarget::~VulkanOffscreenTarget()
    {
        LACRIMA_ASSERT(m_framebuffer == VK_NULL_HANDLE && m_colorImage == VK_NULL_HANDLE,
            "VulkanOffscreenTarget destroyed without calling Shutdown() - GPU resource leak");
    }

    bool VulkanOffscreenTarget::Initialize(VulkanContext& ctx, u32 width, u32 height, VkFormat colorFormat)
    {
        if (width == 0 || height == 0)
        {
            width = 1280;
            height = 720;
        }

        m_width = width;
        m_height = height;
        m_colorFormat = colorFormat;
        m_depthFormat = ctx.FindDepthFormat();

        if (!CreateRenderPass(ctx))
            return false;

        if (!CreateSampler(ctx))
            return false;

        if (!CreateColorResources(ctx))
            return false;

        if (!CreateDepthResources(ctx))
            return false;

        if (!CreateFramebuffer(ctx))
            return false;

        if (!RegisterImGuiDescriptor())
            return false;

        LACRIMA_LOG_INFO(LogCategory::Renderer,
            "VulkanOffscreenTarget initialized successfully ({}x{})", m_width, m_height);
        return true;
    }

    void VulkanOffscreenTarget::DestroyTargetResources(VulkanContext& ctx)
    {
        VkDevice device = ctx.Device();
        if (device == VK_NULL_HANDLE)
            return;

        if (m_descriptorSet != VK_NULL_HANDLE)
        {
            if (ImGui::GetCurrentContext() != nullptr)
            {
                ImGui_ImplVulkan_RemoveTexture(m_descriptorSet);
            }
            m_descriptorSet = VK_NULL_HANDLE;
        }

        if (m_framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(device, m_framebuffer, nullptr);
            m_framebuffer = VK_NULL_HANDLE;
        }

        if (m_depthView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, m_depthView, nullptr);
            m_depthView = VK_NULL_HANDLE;
        }

        if (m_depthImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
        }

        if (m_depthMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, m_depthMemory, nullptr);
            m_depthMemory = VK_NULL_HANDLE;
        }

        if (m_colorView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, m_colorView, nullptr);
            m_colorView = VK_NULL_HANDLE;
        }

        if (m_colorImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(device, m_colorImage, nullptr);
            m_colorImage = VK_NULL_HANDLE;
        }

        if (m_colorMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, m_colorMemory, nullptr);
            m_colorMemory = VK_NULL_HANDLE;
        }
    }

    void VulkanOffscreenTarget::Shutdown(VulkanContext& ctx)
    {
        VkDevice device = ctx.Device();
        if (device == VK_NULL_HANDLE)
            return;

        vkDeviceWaitIdle(device);

        DestroyTargetResources(ctx);

        if (m_sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device, m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }

        if (m_renderPass != VK_NULL_HANDLE)
        {
            vkDestroyRenderPass(device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
    }

    bool VulkanOffscreenTarget::Resize(VulkanContext& ctx, u32 newWidth, u32 newHeight)
    {
        if (newWidth == 0 || newHeight == 0)
            return true;

        if (newWidth == m_width && newHeight == m_height)
            return true;

        VkDevice device = ctx.Device();
        if (device == VK_NULL_HANDLE)
            return false;

        vkDeviceWaitIdle(device);

        DestroyTargetResources(ctx);

        m_width = newWidth;
        m_height = newHeight;

        if (!CreateColorResources(ctx))
            return false;

        if (!CreateDepthResources(ctx))
            return false;

        if (!CreateFramebuffer(ctx))
            return false;

        if (!RegisterImGuiDescriptor())
            return false;

        LACRIMA_LOG_INFO(LogCategory::Renderer,
            "VulkanOffscreenTarget resized to {}x{}", m_width, m_height);
        return true;
    }

    bool VulkanOffscreenTarget::CreateRenderPass(VulkanContext& ctx)
    {
        std::vector<VkAttachmentDescription> attachments;

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = m_colorFormat;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        attachments.push_back(colorAttachment);

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorAttachmentRef;

        VkAttachmentDescription depthAttachment{};
        VkAttachmentReference depthAttachmentRef{};
        if (m_depthFormat != VK_FORMAT_UNDEFINED)
        {
            depthAttachment.format         = m_depthFormat;
            depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
            depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
            depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            attachments.push_back(depthAttachment);

            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
        }

        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass    = 0;
        dependencies[0].srcStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass    = 0;
        dependencies[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
        renderPassInfo.pAttachments    = attachments.data();
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = static_cast<u32>(dependencies.size());
        renderPassInfo.pDependencies   = dependencies.data();

        if (vkCreateRenderPass(ctx.Device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to create VkRenderPass");
            return false;
        }

        return true;
    }

    bool VulkanOffscreenTarget::CreateColorResources(VulkanContext& ctx)
    {
        VkDevice device = ctx.Device();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = m_width;
        imageInfo.extent.height = m_height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = m_colorFormat;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &m_colorImage) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to create color image");
            return false;
        }

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, m_colorImage, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = ctx.FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_colorMemory) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to allocate color memory");
            return false;
        }

        vkBindImageMemory(device, m_colorImage, m_colorMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = m_colorImage;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = m_colorFormat;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &m_colorView) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to create color image view");
            return false;
        }

        return true;
    }

    bool VulkanOffscreenTarget::CreateDepthResources(VulkanContext& ctx)
    {
        if (m_depthFormat == VK_FORMAT_UNDEFINED)
            return true;

        VkDevice device = ctx.Device();

        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = m_width;
        imageInfo.extent.height = m_height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = m_depthFormat;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to create depth image");
            return false;
        }

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(device, m_depthImage, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReqs.size;
        allocInfo.memoryTypeIndex = ctx.FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to allocate depth memory");
            return false;
        }

        vkBindImageMemory(device, m_depthImage, m_depthMemory, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = m_depthImage;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = m_depthFormat;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &m_depthView) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to create depth image view");
            return false;
        }

        return true;
    }

    bool VulkanOffscreenTarget::CreateSampler(VulkanContext& ctx)
    {
        VkDevice device = ctx.Device();

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor   = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to create sampler");
            return false;
        }

        return true;
    }

    bool VulkanOffscreenTarget::CreateFramebuffer(VulkanContext& ctx)
    {
        std::vector<VkImageView> attachments = { m_colorView };
        if (m_depthView != VK_NULL_HANDLE)
        {
            attachments.push_back(m_depthView);
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = m_renderPass;
        fbInfo.attachmentCount = static_cast<u32>(attachments.size());
        fbInfo.pAttachments    = attachments.data();
        fbInfo.width           = m_width;
        fbInfo.height          = m_height;
        fbInfo.layers          = 1;

        if (vkCreateFramebuffer(ctx.Device(), &fbInfo, nullptr, &m_framebuffer) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to create framebuffer");
            return false;
        }

        return true;
    }

    bool VulkanOffscreenTarget::RegisterImGuiDescriptor()
    {
        if (ImGui::GetCurrentContext() == nullptr)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: ImGui context is null");
            return false;
        }

        m_descriptorSet = ImGui_ImplVulkan_AddTexture(m_sampler, m_colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        if (m_descriptorSet == VK_NULL_HANDLE)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanOffscreenTarget: failed to register ImGui texture descriptor");
            return false;
        }
        return true;
    }
}
