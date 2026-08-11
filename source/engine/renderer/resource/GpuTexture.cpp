#include "renderer/resource/GpuTexture.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMemoryAllocator.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include <fstream>

namespace dt::renderer
{
    GpuTexture::~GpuTexture()
    {
        DT_ASSERT(m_image == VK_NULL_HANDLE, "GpuTexture destroyed without calling Shutdown()");
    }

    bool GpuTexture::Initialize(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                                u32 width, u32 height, VkFormat format)
    {
        m_width = width;
        m_height = height;
        m_format = format;

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        
        if (vkCreateImage(ctx.Device(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuTexture: failed to create VkImage");
            return false;
        }

        m_memory = allocator.AllocateImageMemory(m_image, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (m_memory == VK_NULL_HANDLE)
        {
            vkDestroyImage(ctx.Device(), m_image, nullptr);
            m_image = VK_NULL_HANDLE;
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(ctx.Device(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuTexture: failed to create VkImageView");
            return false;
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(ctx.Device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuTexture: failed to create VkSampler");
            return false;
        }

        return true;
    }

    void GpuTexture::Shutdown(VulkanContext& ctx, const VulkanMemoryAllocator& allocator)
    {
        if (m_sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(ctx.Device(), m_sampler, nullptr);
            m_sampler = VK_NULL_HANDLE;
        }

        if (m_imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(ctx.Device(), m_imageView, nullptr);
            m_imageView = VK_NULL_HANDLE;
        }

        if (m_image != VK_NULL_HANDLE)
        {
            vkDestroyImage(ctx.Device(), m_image, nullptr);
            m_image = VK_NULL_HANDLE;
        }

        if (m_memory != VK_NULL_HANDLE)
        {
            allocator.FreeMemory(m_memory);
            m_memory = VK_NULL_HANDLE;
        }
    }

    static void TransitionImageLayout(VulkanContext& ctx, VkImage image, VkFormat /*format*/, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer cmd = ctx.BeginOneTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            DT_ASSERT(false, "Unsupported layout transition");
            ctx.EndOneTimeCommands(cmd);
            return;
        }

        vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        ctx.EndOneTimeCommands(cmd);
    }

    bool GpuTexture::LoadFromCookedFile(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                                        const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuTexture: failed to open file '{}'", path);
            return false;
        }

        struct AssetHeader {
            char magic[4];
            u32 version;
            u32 type;
        } header;

        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic[0] != 'D' || header.magic[1] != 'T' || header.magic[2] != 'A' || header.magic[3] != 'S')
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuTexture: invalid magic in file '{}'", path);
            return false;
        }
        if (header.type != 1)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "GpuTexture: asset type is not texture in file '{}'", path);
            return false;
        }

        struct TexturePayloadHeader {
            u32 width;
            u32 height;
            u32 channels;
        } texHeader;

        file.read(reinterpret_cast<char*>(&texHeader), sizeof(texHeader));

        u32 dataSize = texHeader.width * texHeader.height * 4;
        std::vector<u8> pixels(dataSize);
        file.read(reinterpret_cast<char*>(pixels.data()), dataSize);

        if (!Initialize(ctx, allocator, texHeader.width, texHeader.height, VK_FORMAT_R8G8B8A8_UNORM))
        {
            return false;
        }

        VulkanBuffer stagingBuffer;
        if (!stagingBuffer.Initialize(ctx, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Shutdown(ctx, allocator);
            return false;
        }

        stagingBuffer.CopyData(ctx, pixels.data(), dataSize);

        TransitionImageLayout(ctx, m_image, m_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkCommandBuffer cmd = ctx.BeginOneTimeCommands();
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {texHeader.width, texHeader.height, 1};

        vkCmdCopyBufferToImage(cmd, stagingBuffer.Handle(), m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        ctx.EndOneTimeCommands(cmd);

        TransitionImageLayout(ctx, m_image, m_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        stagingBuffer.Shutdown(ctx);

        return true;
    }
}
