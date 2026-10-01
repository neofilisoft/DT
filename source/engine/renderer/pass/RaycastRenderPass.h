// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "renderer/graph/RenderGraph.h"
#include "renderer/vulkan/VulkanPipeline.h"
#include "renderer/vulkan/VulkanShader.h"
#include "runtime/SimulationSnapshot.h"

namespace lacrima::renderer
{
    class VulkanContext;
    class VulkanMaterial;

    // Renders one vertical wall slice per RaycastColumn. The bound material
    // uses a point-filtered atlas; textureIndex selects an atlas row.
    class RaycastRenderPass final : public RenderPass
    {
    public:
        RaycastRenderPass();
        ~RaycastRenderPass() override = default;

        bool Initialize(VulkanContext& ctx,
                        VkRenderPass renderPass,
                        VkDescriptorSetLayout uboLayout,
                        VkDescriptorSetLayout materialLayout);
        void Shutdown(VulkanContext& ctx);

        void SetAtlasRowCount(u32 rows) { m_atlasRows = rows == 0 ? 1u : rows; }

        void SetupFrame(VkExtent2D extent,
                        VkDescriptorSet globalUboSet,
                        const VulkanMaterial* material,
                        const std::vector<RaycastColumn>* columns);
        void Execute(VkCommandBuffer cmd) override;

    private:
        VulkanShader m_vertShader;
        VulkanShader m_fragShader;
        VulkanPipeline m_pipeline;
        VkExtent2D m_extent{};
        VkDescriptorSet m_globalUboSet = VK_NULL_HANDLE;
        const VulkanMaterial* m_material = nullptr;
        const std::vector<RaycastColumn>* m_columns = nullptr;
        u32 m_atlasRows = 1;
    };
}
