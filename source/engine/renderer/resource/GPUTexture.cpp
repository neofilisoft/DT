// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/resource/GPUTexture.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMemoryAllocator.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include <fstream>
#include <zlib.h>

namespace lacrima::renderer
{
    GPUTexture::~GPUTexture()
    {
        LACRIMA_ASSERT(m_image == VK_NULL_HANDLE, "GPUTexture destroyed without calling Shutdown()");
    }

    bool GPUTexture::Initialize(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                                u32 width, u32 height, VkFormat format, bool pixelPerfect)
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
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: failed to create VkImage");
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
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: failed to create VkImageView");
            return false;
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = pixelPerfect ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        samplerInfo.minFilter = pixelPerfect ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
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
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: failed to create VkSampler");
            return false;
        }

        return true;
    }

    void GPUTexture::Shutdown(VulkanContext& ctx, const VulkanMemoryAllocator& allocator)
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
            LACRIMA_ASSERT(false, "Unsupported layout transition");
            ctx.EndOneTimeCommands(cmd);
            return;
        }

        vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        ctx.EndOneTimeCommands(cmd);
    }

    static void CopyBufferToImage(VulkanContext& ctx, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
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
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        ctx.EndOneTimeCommands(cmd);
    }

    bool GPUTexture::LoadFromMemory(VulkanContext& ctx, const VulkanMemoryAllocator& allocator, const void* data, u32 width, u32 height, VkFormat format, bool pixelPerfect)
    {
        if (!Initialize(ctx, allocator, width, height, format, pixelPerfect))
        {
            return false;
        }

        u32 bpp = 4; // Assuming R8G8B8A8
        if (format == VK_FORMAT_R8G8B8A8_UNORM) bpp = 4;
        // simplistic BPP calculation for fallback logic
        
        u32 dataSize = width * height * bpp;

        VulkanBuffer stagingBuffer;
        if (!stagingBuffer.Initialize(ctx, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            Shutdown(ctx, allocator);
            return false;
        }

        stagingBuffer.CopyData(ctx, data, dataSize);

        TransitionImageLayout(ctx, m_image, m_format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(ctx, stagingBuffer.Handle(), m_image, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        TransitionImageLayout(ctx, m_image, m_format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        stagingBuffer.Shutdown(ctx);

        return true;
    }

    bool GPUTexture::LoadFromCookedFile(VulkanContext& ctx, const VulkanMemoryAllocator& allocator,
                                        const std::string& path, bool pixelPerfect)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: failed to open file '{}'", path);
            return false;
        }

        struct AssetHeader {
            char magic[4];
            u32 version;
            u32 type;
            u32 isCompressed;
            u32 uncompressedSize;
        } header;

        file.read(reinterpret_cast<char*>(&header), sizeof(header));
        if (header.magic[0] != 'D' || header.magic[1] != 'T' || header.magic[2] != 'A' || header.magic[3] != 'S')
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: invalid magic in file '{}'", path);
            return false;
        }
        if (header.version != 2)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: unsupported asset version {} in file '{}' (expected 2)", header.version, path);
            return false;
        }
        if (header.type != 1)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: asset type is not texture in file '{}'", path);
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
        
        if (header.isCompressed == 1)
        {
            std::streamsize compressedSize = file.seekg(0, std::ios::end).tellg() - static_cast<std::streampos>(sizeof(header) + sizeof(texHeader));
            file.seekg(sizeof(header) + sizeof(texHeader), std::ios::beg);
            
            std::vector<uint8_t> compressedData(compressedSize);
            file.read(reinterpret_cast<char*>(compressedData.data()), compressedSize);
            
            uLongf destLen = header.uncompressedSize;
            int zResult = uncompress(pixels.data(), &destLen, compressedData.data(), compressedSize);
            
            if (zResult != Z_OK || destLen != header.uncompressedSize)
            {
                LACRIMA_LOG_ERROR(LogCategory::Renderer, "GPUTexture: failed to decompress payload in file '{}'", path);
                return false;
            }
        }
        else
        {
            file.read(reinterpret_cast<char*>(pixels.data()), dataSize);
        }

        if (!LoadFromMemory(ctx, allocator, pixels.data(), texHeader.width, texHeader.height, VK_FORMAT_R8G8B8A8_UNORM, pixelPerfect))
        {
            return false;
        }

        return true;
    }
}





