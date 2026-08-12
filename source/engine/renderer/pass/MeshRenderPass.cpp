#include "renderer/pass/MeshRenderPass.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMaterial.h"
#include "renderer/resource/GpuMesh.h"
#include "core/logging/Logger.h"
#include "core/math/Math.h"

namespace dt::renderer
{
    struct MeshPushConstants
    {
        Mat4 modelMatrix;
        Vec4 color;
    };

    MeshRenderPass::MeshRenderPass()
        : RenderPass("MeshRenderPass")
    {
    }

    bool MeshRenderPass::Initialize(VulkanContext& ctx, 
                                    VkRenderPass renderPass, 
                                    VkDescriptorSetLayout uboLayout, 
                                    VkDescriptorSetLayout materialLayout)
    {
        if (!m_vertShader.InitializeFromCookedFile(ctx, "source/engine/asset/agent_mesh_vert.asset"))
            return false;
            
        if (!m_fragShader.InitializeFromCookedFile(ctx, "source/engine/asset/agent_mesh_frag.asset"))
            return false;

        VkDescriptorSetLayout layouts[] = { uboLayout, materialLayout };
        
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(MeshPushConstants);

        // Define Vertex Input for GpuMesh::Vertex
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attributeDescriptions[3]{};
        
        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // Normal
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        // TexCoord
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        VulkanPipeline::Config config{};
        config.renderPass = renderPass;
        config.vertShader = &m_vertShader;
        config.fragShader = &m_fragShader;
        config.pDescriptorSetLayouts = layouts;
        config.descriptorSetLayoutCount = 2;
        config.pPushConstantRanges = &pushConstantRange;
        config.pushConstantRangeCount = 1;
        config.enableDepthTest = true;
        config.enableAlphaBlend = true;
        config.pVertexBindings = &bindingDescription;
        config.vertexBindingCount = 1;
        config.pVertexAttributes = attributeDescriptions;
        config.vertexAttributeCount = 3;

        if (!m_pipeline.Initialize(ctx, config))
        {
            return false;
        }

        return true;
    }

    void MeshRenderPass::Shutdown(VulkanContext& ctx)
    {
        m_pipeline.Shutdown(ctx);
        m_fragShader.Shutdown(ctx);
        m_vertShader.Shutdown(ctx);
    }

    void MeshRenderPass::SetupFrame(VkExtent2D extent, 
                                    VkDescriptorSet globalUboSet, 
                                    const VulkanMaterial* material, 
                                    const GpuMesh* mesh,
                                    const std::vector<RenderProxy>* proxies)
    {
        m_extent = extent;
        m_globalUboSet = globalUboSet;
        m_material = material;
        m_mesh = mesh;
        m_proxies = proxies;
    }

    void MeshRenderPass::Execute(VkCommandBuffer cmd)
    {
        if (!m_pipeline.IsInitialized() || !m_proxies || !m_material || !m_mesh)
            return;
            
        if (m_mesh->GetVertexBuffer() == VK_NULL_HANDLE || m_mesh->GetIndexBuffer() == VK_NULL_HANDLE)
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

        VkBuffer vertexBuffers[] = { m_mesh->GetVertexBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, m_mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        const uint32_t indexCount = m_mesh->GetIndexCount();

        for (const auto& proxy : *m_proxies)
        {
            MeshPushConstants pc{};
            pc.modelMatrix = Mat4::Translation(Vec3(proxy.positionX, proxy.positionY, proxy.positionZ));

            const u32 colorIndex = proxy.visualId % 6;
            if (colorIndex == 0)
            {
                pc.color = Vec4(0.2f, 0.8f, 0.4f, 1.0f);
            }
            else if (colorIndex == 1)
            {
                pc.color = Vec4(0.8f, 0.4f, 0.2f, 1.0f);
            }
            else
            {
                pc.color = Vec4(0.5f, 0.5f, 0.5f, 1.0f);
            }

            vkCmdPushConstants(cmd, m_pipeline.Layout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &pc);
            vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
        }
    }
}
