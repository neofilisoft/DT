#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

#include <string>

// ---------------------------------------------------------------------------
// VulkanShader.h
//
// RAII wrapper around a VkShaderModule.
//
// In Vulkan, shaders are loaded as binary SPIR-V bytecode. A VkShaderModule
// is created from this bytecode and passed to the VkGraphicsPipelineCreateInfo
// shader stage description.
//
// This wrapper can create a shader module either by loading a `.spv` file
// from disk or by using an in-memory array of precompiled SPIR-V bytecode
// (which we use for the agent quad shaders to avoid file path issues at runtime).
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;

    class VulkanShader
    {
    public:
        VulkanShader() = default;
        ~VulkanShader();

        VulkanShader(const VulkanShader&)            = delete;
        VulkanShader& operator=(const VulkanShader&) = delete;
        VulkanShader(VulkanShader&&)                 = delete;
        VulkanShader& operator=(VulkanShader&&)      = delete;

        // Creates the shader module from a `.spv` file on disk. (Legacy)
        bool InitializeFromFile(VulkanContext& ctx, const std::string& filepath);

        // Creates the shader module from a `.asset` file cooked by DTCooker
        bool InitializeFromCookedFile(VulkanContext& ctx, const std::string& filepath);

        // Creates the shader module from an in-memory bytecode array.
        // sizeBytes must be a multiple of 4.
        bool InitializeFromMemory(VulkanContext& ctx, const u32* code, usize sizeBytes);

        void Shutdown(VulkanContext& ctx);

        VkShaderModule Handle() const { return m_module; }
        VkShaderStageFlagBits GetStage() const { return m_stage; }
        bool IsInitialized() const { return m_module != VK_NULL_HANDLE; }

    private:
        VkShaderModule m_module = VK_NULL_HANDLE;
        VkShaderStageFlagBits m_stage = VK_SHADER_STAGE_VERTEX_BIT;
    };
}
