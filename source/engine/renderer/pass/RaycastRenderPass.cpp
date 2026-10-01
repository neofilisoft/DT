// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/pass/RaycastRenderPass.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMaterial.h"
#include "core/filesystem/FileSystem.h"

#include <algorithm>

namespace lacrima::renderer
{
    struct RaycastPushConstants
    {
        float geometry[4]; // ndc center x, half width, half height, center y
        float uv[4];       // wall U, atlas row, shade, unused
        float color[4];
    };

    RaycastRenderPass::RaycastRenderPass()
        : RenderPass("RaycastRenderPass")
    {
    }

    bool RaycastRenderPass::Initialize(VulkanContext& ctx,
                                       VkRenderPass renderPass,
                                       VkDescriptorSetLayout uboLayout,
                                       VkDescriptorSetLayout materialLayout)
    {
        if (!m_vertShader.InitializeFromCookedFile(ctx, FileSystem::GetEngineAssetDir() + "/raycast_column_vert.asset"))
            return false;
        if (!m_fragShader.InitializeFromCookedFile(ctx, FileSystem::GetEngineAssetDir() + "/raycast_column_frag.asset"))
            return false;

        VkDescriptorSetLayout layouts[] = { uboLayout, materialLayout };
        VkPushConstantRange push{};
        push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        push.size = sizeof(RaycastPushConstants);

        VulkanPipeline::Config config{};
        config.renderPass = renderPass;
        config.vertShader = &m_vertShader;
        config.fragShader = &m_fragShader;
        config.pDescriptorSetLayouts = layouts;
        config.descriptorSetLayoutCount = 2;
        config.pPushConstantRanges = &push;
        config.pushConstantRangeCount = 1;
        config.enableDepthTest = false;
        config.enableAlphaBlend = false;
        return m_pipeline.Initialize(ctx, config);
    }

    void RaycastRenderPass::Shutdown(VulkanContext& ctx)
    {
        m_pipeline.Shutdown(ctx);
        m_fragShader.Shutdown(ctx);
        m_vertShader.Shutdown(ctx);
    }

    void RaycastRenderPass::SetupFrame(VkExtent2D extent,
                                       VkDescriptorSet globalUboSet,
                                       const VulkanMaterial* material,
                                       const std::vector<RaycastColumn>* columns)
    {
        m_extent = extent;
        m_globalUboSet = globalUboSet;
        m_material = material;
        m_columns = columns;
    }

    void RaycastRenderPass::Execute(VkCommandBuffer cmd)
    {
        if (!m_pipeline.IsInitialized() || !m_columns || m_columns->empty() ||
            !m_material || !m_material->IsInitialized())
            return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.Handle());
        VkViewport viewport{0.0f, 0.0f, static_cast<float>(m_extent.width),
                            static_cast<float>(m_extent.height), 0.0f, 1.0f};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        VkRect2D scissor{{0, 0}, m_extent};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDescriptorSet sets[] = { m_globalUboSet, m_material->GetDescriptorSet() };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipeline.Layout(), 0, 2, sets, 0, nullptr);

        const float columnCount = static_cast<float>(m_columns->size());
        for (usize i = 0; i < m_columns->size(); ++i)
        {
            const RaycastColumn& column = (*m_columns)[i];
            if (!column.hit)
                continue;

            const float distance = std::max(column.distance, 0.01f);
            const float centerX = -1.0f + 2.0f * (static_cast<float>(i) + 0.5f) / columnCount;
            const float halfWidth = 1.0f / columnCount;
            const float halfHeight = std::clamp(0.75f / distance, 0.015f, 1.0f);

            RaycastPushConstants pc{};
            pc.geometry[0] = centerX;
            pc.geometry[1] = halfWidth;
            pc.geometry[2] = halfHeight;
            pc.geometry[3] = 0.0f;
            pc.uv[0] = std::clamp(column.wallU, 0.0f, 1.0f);
            pc.uv[1] = static_cast<float>(column.textureIndex > 0 ? column.textureIndex - 1 : 0);
            pc.uv[2] = std::clamp(column.shade, 0.0f, 1.0f);
            pc.uv[3] = static_cast<float>(m_atlasRows);
            pc.color[0] = pc.color[1] = pc.color[2] = 1.0f;
            pc.color[3] = 1.0f;

            vkCmdPushConstants(cmd, m_pipeline.Layout(),
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(pc), &pc);
            vkCmdDraw(cmd, 6, 1, 0, 0);
        }
    }
}
