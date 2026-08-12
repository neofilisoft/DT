#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
// VulkanPipeline.h
//
// Encapsulates a VkPipeline, VkPipelineLayout, and VkDescriptorSetLayout.
//
// Specifically tailored for quad rendering:
//   - No vertex input attributes (vertices are reconstructed in shader).
//   - Color blending enabled (standard alpha blending).
//   - Dynamic viewport and scissor states.
//   - Orthographic projection input via descriptor set layout binding 0 (UBO).
//   - Push constant range for per-agent position, size, and color.
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;
    class VulkanShader;

    class VulkanPipeline
    {
    public:
        VulkanPipeline() = default;
        ~VulkanPipeline();

        VulkanPipeline(const VulkanPipeline&)            = delete;
        VulkanPipeline& operator=(const VulkanPipeline&) = delete;
        VulkanPipeline(VulkanPipeline&&)                 = delete;
        VulkanPipeline& operator=(VulkanPipeline&&)      = delete;

        struct Config
        {
            VkRenderPass renderPass = VK_NULL_HANDLE;
            VulkanShader* vertShader = nullptr;
            VulkanShader* fragShader = nullptr;
            const VkDescriptorSetLayout* pDescriptorSetLayouts = nullptr;
            u32 descriptorSetLayoutCount = 0;
            const VkPushConstantRange* pPushConstantRanges = nullptr;
            u32 pushConstantRangeCount = 0;
            
            bool enableDepthTest = false;
            bool enableAlphaBlend = true;
            
            // Vertex input
            const VkVertexInputBindingDescription* pVertexBindings = nullptr;
            u32 vertexBindingCount = 0;
            const VkVertexInputAttributeDescription* pVertexAttributes = nullptr;
            u32 vertexAttributeCount = 0;
        };

        bool Initialize(VulkanContext& ctx, const Config& config);

        void Shutdown(VulkanContext& ctx);

        VkPipeline            Handle()              const { return m_pipeline; }
        VkPipelineLayout      Layout()              const { return m_layout; }

        bool IsInitialized() const { return m_pipeline != VK_NULL_HANDLE; }

    private:
        VkPipeline            m_pipeline            = VK_NULL_HANDLE;
        VkPipelineLayout      m_layout              = VK_NULL_HANDLE;
    };
}
