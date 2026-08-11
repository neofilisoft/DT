#pragma once

#include "renderer/graph/RenderGraph.h"
#include "renderer/vulkan/VulkanPipeline.h"
#include "renderer/vulkan/VulkanShader.h"
#include "runtime/SimulationSnapshot.h"

namespace dt::renderer
{
    class VulkanContext;
    class VulkanMaterial;
    class GpuMesh;

    class MeshRenderPass : public RenderPass
    {
    public:
        MeshRenderPass();
        ~MeshRenderPass() override = default;

        bool Initialize(VulkanContext& ctx, 
                        VkRenderPass renderPass, 
                        VkDescriptorSetLayout uboLayout, 
                        VkDescriptorSetLayout materialLayout);
        
        void Shutdown(VulkanContext& ctx);

        void SetupFrame(VkExtent2D extent, 
                        VkDescriptorSet globalUboSet, 
                        const VulkanMaterial* material, 
                        const GpuMesh* mesh,
                        const std::vector<RenderProxy>* proxies);

        void Execute(VkCommandBuffer cmd) override;

        bool IsInitialized() const { return m_pipeline.IsInitialized(); }

    private:
        VulkanShader m_vertShader;
        VulkanShader m_fragShader;
        VulkanPipeline m_pipeline;

        // Frame data
        VkExtent2D m_extent;
        VkDescriptorSet m_globalUboSet = VK_NULL_HANDLE;
        const VulkanMaterial* m_material = nullptr;
        const GpuMesh* m_mesh = nullptr;
        const std::vector<RenderProxy>* m_proxies = nullptr;
    };
}
