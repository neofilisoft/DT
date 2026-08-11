#include "renderer/vulkan/VulkanShader.h"

#include "core/logging/Logger.h"
#include "core/platform/Assert.h"
#include "renderer/vulkan/VulkanContext.h"

#include <fstream>
#include <vector>

namespace dt::renderer
{
    VulkanShader::~VulkanShader()
    {
        DT_ASSERT(m_module == VK_NULL_HANDLE,
            "VulkanShader destroyed without calling Shutdown() - resource leak");
    }

    bool VulkanShader::InitializeFromFile(VulkanContext& ctx, const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanShader: failed to open file '{}'", filepath);
            return false;
        }

        const size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();

        return InitializeFromMemory(ctx, reinterpret_cast<const u32*>(buffer.data()), fileSize);
    }

    bool VulkanShader::InitializeFromCookedFile(VulkanContext& ctx, const std::string& filepath)
    {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanShader: failed to open asset file '{}'", filepath);
            return false;
        }

        const size_t fileSize = static_cast<size_t>(file.tellg());
        file.seekg(0);

        struct AssetHeader {
            char magic[4];
            u32 version;
            u32 type;
        } header;

        if (fileSize < sizeof(header)) return false;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic[0] != 'D' || header.magic[1] != 'T' || header.magic[2] != 'A' || header.magic[3] != 'S')
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanShader: invalid magic in asset '{}'", filepath);
            return false;
        }

        if (header.type != 3) // 3 = Shader
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanShader: asset is not a shader '{}'", filepath);
            return false;
        }

        struct ShaderPayloadHeader {
            u32 byteSize;
            u32 stage;
        } payload;

        if (fileSize < sizeof(header) + sizeof(payload)) return false;
        file.read(reinterpret_cast<char*>(&payload), sizeof(payload));

        std::vector<char> buffer(payload.byteSize);
        if (fileSize < sizeof(header) + sizeof(payload) + payload.byteSize) return false;
        file.read(buffer.data(), payload.byteSize);

        if (payload.stage == 0) m_stage = VK_SHADER_STAGE_VERTEX_BIT;
        else if (payload.stage == 1) m_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        else if (payload.stage == 2) m_stage = VK_SHADER_STAGE_COMPUTE_BIT;
        else m_stage = VK_SHADER_STAGE_VERTEX_BIT;

        return InitializeFromMemory(ctx, reinterpret_cast<const u32*>(buffer.data()), payload.byteSize);
    }

    bool VulkanShader::InitializeFromMemory(VulkanContext& ctx, const u32* code, usize sizeBytes)
    {
        DT_ASSERT(sizeBytes % 4 == 0,
            "VulkanShader::InitializeFromMemory: sizeBytes must be a multiple of 4");

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = sizeBytes;
        createInfo.pCode    = code;

        if (vkCreateShaderModule(ctx.Device(), &createInfo, nullptr, &m_module) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanShader: failed to create VkShaderModule");
            return false;
        }

        return true;
    }

    void VulkanShader::Shutdown(VulkanContext& ctx)
    {
        if (ctx.Device() == VK_NULL_HANDLE)
            return;

        if (m_module != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(ctx.Device(), m_module, nullptr);
            m_module = VK_NULL_HANDLE;
        }
    }
}
