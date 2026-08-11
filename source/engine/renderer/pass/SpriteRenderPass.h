#pragma once

#include "renderer/graph/RenderGraph.h"
#include "renderer/vulkan/VulkanPipeline.h"
#include "renderer/vulkan/VulkanShader.h"
#include "runtime/SimulationSnapshot.h"

namespace dt::renderer
{
    class VulkanContext;
    class VulkanMaterial;

    class SpriteRenderPass : public RenderPass
    {
    public:
        SpriteRenderPass();
        ~SpriteRenderPass() override = default;

        bool Initialize(VulkanContext& ctx, 
                        VkRenderPass renderPass, 
                        VkDescriptorSetLayout uboLayout, 
                        VkDescriptorSetLayout materialLayout);
        
        void Shutdown(VulkanContext& ctx);

        // Sets the frame data required before Execute is called by RenderGraph
        void SetupFrame(VkExtent2D extent, 
                        VkDescriptorSet globalUboSet, 
                        const VulkanMaterial* material, 
                        const std::vector<RenderProxy>* proxies);

        void Execute(VkCommandBuffer cmd) override;

    private:
        VulkanShader m_vertShader;
        VulkanShader m_fragShader;
        VulkanPipeline m_pipeline;

        // Frame data
        VkExtent2D m_extent;
        VkDescriptorSet m_globalUboSet = VK_NULL_HANDLE;
        const VulkanMaterial* m_material = nullptr;
        const std::vector<RenderProxy>* m_proxies = nullptr;
    };
}
