#include "renderer/pass/SpriteRenderPass.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMaterial.h"
#include "core/logging/Logger.h"
#include "core/math/Math.h"

namespace dt::renderer
{
    // Matches push constant definition in agent_quad.vert
    struct SpritePushConstants
    {
        float worldPos[2];   // 8 bytes
        float halfExtent[2]; // 8 bytes
        float color[4];      // 16 bytes
    };

    SpriteRenderPass::SpriteRenderPass()
        : RenderPass("SpriteRenderPass")
    {
    }

    bool SpriteRenderPass::Initialize(VulkanContext& ctx, 
                                      VkRenderPass renderPass, 
                                      VkDescriptorSetLayout uboLayout, 
                                      VkDescriptorSetLayout materialLayout)
    {
        if (!m_vertShader.InitializeFromCookedFile(ctx, "source/engine/asset/agent_quad_vert.asset"))
            return false;
            
        if (!m_fragShader.InitializeFromCookedFile(ctx, "source/engine/asset/agent_quad_frag.asset"))
            return false;

        VkDescriptorSetLayout layouts[] = { uboLayout, materialLayout };
        
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(SpritePushConstants);

        VulkanPipeline::Config config{};
        config.renderPass = renderPass;
        config.vertShader = &m_vertShader;
        config.fragShader = &m_fragShader;
        config.pDescriptorSetLayouts = layouts;
        config.descriptorSetLayoutCount = 2;
        config.pPushConstantRanges = &pushConstantRange;
        config.pushConstantRangeCount = 1;
        config.enableDepthTest = false;
        config.enableAlphaBlend = true;

        if (!m_pipeline.Initialize(ctx, config))
        {
            return false;
        }

        return true;
    }

    void SpriteRenderPass::Shutdown(VulkanContext& ctx)
    {
        m_pipeline.Shutdown(ctx);
        m_fragShader.Shutdown(ctx);
        m_vertShader.Shutdown(ctx);
    }

    void SpriteRenderPass::SetupFrame(VkExtent2D extent, 
                                      VkDescriptorSet globalUboSet, 
                                      const VulkanMaterial* material, 
                                      const std::vector<RenderProxy>* proxies)
    {
        m_extent = extent;
        m_globalUboSet = globalUboSet;
        m_material = material;
        m_proxies = proxies;
    }

    void SpriteRenderPass::Execute(VkCommandBuffer cmd)
    {
        if (!m_pipeline.IsInitialized() || !m_proxies || !m_material)
            return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.Handle());

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(m_extent.width);
        viewport.height   = static_cast<float>(m_extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = m_extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDescriptorSet sets[] = { m_globalUboSet, m_material->GetDescriptorSet() };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.Layout(), 0, 2, sets, 0, nullptr);

        for (const auto& proxy : *m_proxies)
        {
            SpritePushConstants pc{};
            pc.worldPos[0] = proxy.positionX;
            pc.worldPos[1] = proxy.positionZ; // Map 3D XZ to 2D XY
            pc.halfExtent[0] = 0.5f;
            pc.halfExtent[1] = 0.5f;

            const u32 colorIndex = proxy.archetypeId % 6;
            if (colorIndex == 0)
            {
                pc.color[0] = 0.2f; pc.color[1] = 0.8f; pc.color[2] = 0.4f; pc.color[3] = 1.0f;
            }
            else if (colorIndex == 1)
            {
                pc.color[0] = 0.8f; pc.color[1] = 0.4f; pc.color[2] = 0.2f; pc.color[3] = 1.0f;
            }
            else
            {
                pc.color[0] = 0.5f; pc.color[1] = 0.5f; pc.color[2] = 0.5f; pc.color[3] = 1.0f;
            }

            vkCmdPushConstants(cmd, m_pipeline.Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(SpritePushConstants), &pc);
            vkCmdDraw(cmd, 6, 1, 0, 0);
        }
    }
}
