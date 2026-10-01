// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/pass/SkinnedMeshPass.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanMaterial.h"
#include "renderer/resource/GPUMesh.h"
#include "core/logging/Logger.h"
#include "core/filesystem/FileSystem.h"
#include "core/math/Math.h"
#include <algorithm>
#include <cstring>

namespace lacrima::renderer
{
    struct SkinnedPushConstants
    {
        Mat4 modelMatrix;
        Vec4 color;
    };

    SkinnedMeshPass::SkinnedMeshPass()
        : RenderPass("SkinnedMeshPass")
    {
    }

    bool SkinnedMeshPass::CreateBonePaletteLayout(VulkanContext& ctx)
    {
        // Descriptor Set Layout for bone palette storage buffer (set=2, binding=0)
        VkDescriptorSetLayoutBinding boneBinding{};
        boneBinding.binding         = 0;
        boneBinding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        boneBinding.descriptorCount = 1;
        boneBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &boneBinding;

        if (vkCreateDescriptorSetLayout(ctx.Device(), &layoutInfo, nullptr, &m_bonePaletteLayout) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "SkinnedMeshPass: failed to create bone palette descriptor set layout");
            return false;
        }

        // Small descriptor pool for the bone SSBO (one set, one storage buffer)
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;

        if (vkCreateDescriptorPool(ctx.Device(), &poolInfo, nullptr, &m_boneDescPool) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "SkinnedMeshPass: failed to create bone descriptor pool");
            return false;
        }

        // Allocate the one descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_boneDescPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_bonePaletteLayout;

        if (vkAllocateDescriptorSets(ctx.Device(), &allocInfo, &m_bonePaletteSet) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "SkinnedMeshPass: failed to allocate bone descriptor set");
            return false;
        }

        // Create host-visible storage buffer for the bone palette
        const VkDeviceSize bufferSize = sizeof(Mat4) * kMaxBones;
        if (!m_bonePaletteBuffer.Initialize(
                ctx,
                bufferSize,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "SkinnedMeshPass: failed to create bone palette buffer");
            return false;
        }

        // Bind the buffer to the descriptor set
        VkDescriptorBufferInfo bufInfo{};
        bufInfo.buffer = m_bonePaletteBuffer.Handle();
        bufInfo.offset = 0;
        bufInfo.range  = bufferSize;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_bonePaletteSet;
        write.dstBinding      = 0;
        write.descriptorCount = 1;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.pBufferInfo     = &bufInfo;

        vkUpdateDescriptorSets(ctx.Device(), 1, &write, 0, nullptr);
        return true;
    }

    bool SkinnedMeshPass::Initialize(
        VulkanContext&        ctx,
        VkRenderPass          renderPass,
        VkDescriptorSetLayout uboLayout,
        VkDescriptorSetLayout materialLayout)
    {
        if (!m_vertShader.InitializeFromCookedFile(ctx, FileSystem::GetEngineAssetDir() + "/skinned_mesh_vert.asset"))
            return false;
        if (!m_fragShader.InitializeFromCookedFile(ctx, FileSystem::GetEngineAssetDir() + "/skinned_mesh_frag.asset"))
            return false;

        if (!CreateBonePaletteLayout(ctx))
            return false;

        VkDescriptorSetLayout layouts[] = { uboLayout, materialLayout, m_bonePaletteLayout };

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = sizeof(SkinnedPushConstants);

        // Vertex layout for SkinnedVertex
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding   = 0;
        bindingDesc.stride    = sizeof(SkinnedVertex);
        bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrs[6]{};
        // position (location 0)
        attrs[0].binding  = 0; attrs[0].location = 0;
        attrs[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[0].offset   = offsetof(SkinnedVertex, position);
        // normal (location 1)
        attrs[1].binding  = 0; attrs[1].location = 1;
        attrs[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        attrs[1].offset   = offsetof(SkinnedVertex, normal);
        // texCoord (location 2)
        attrs[2].binding  = 0; attrs[2].location = 2;
        attrs[2].format   = VK_FORMAT_R32G32_SFLOAT;
        attrs[2].offset   = offsetof(SkinnedVertex, texCoord);
        // tangent (location 3)
        attrs[3].binding  = 0; attrs[3].location = 3;
        attrs[3].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[3].offset   = offsetof(SkinnedVertex, tangent);
        // boneIndices (location 4) - uint
        attrs[4].binding  = 0; attrs[4].location = 4;
        attrs[4].format   = VK_FORMAT_R32G32B32A32_UINT;
        attrs[4].offset   = offsetof(SkinnedVertex, boneIndices);
        // boneWeights (location 5)
        attrs[5].binding  = 0; attrs[5].location = 5;
        attrs[5].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[5].offset   = offsetof(SkinnedVertex, boneWeights);

        VulkanPipeline::Config config{};
        config.renderPass                = renderPass;
        config.vertShader                = &m_vertShader;
        config.fragShader                = &m_fragShader;
        config.pDescriptorSetLayouts     = layouts;
        config.descriptorSetLayoutCount  = 3;
        config.pPushConstantRanges       = &pushConstantRange;
        config.pushConstantRangeCount    = 1;
        config.enableDepthTest           = true;
        config.enableAlphaBlend          = true;
        config.pVertexBindings           = &bindingDesc;
        config.vertexBindingCount        = 1;
        config.pVertexAttributes         = attrs;
        config.vertexAttributeCount      = 6;

        return m_pipeline.Initialize(ctx, config);
    }

    void SkinnedMeshPass::Shutdown(VulkanContext& ctx)
    {
        m_bonePaletteBuffer.Shutdown(ctx);
        if (m_boneDescPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(ctx.Device(), m_boneDescPool, nullptr);
            m_boneDescPool = VK_NULL_HANDLE;
        }
        if (m_bonePaletteLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(ctx.Device(), m_bonePaletteLayout, nullptr);
            m_bonePaletteLayout = VK_NULL_HANDLE;
        }
        m_pipeline.Shutdown(ctx);
        m_fragShader.Shutdown(ctx);
        m_vertShader.Shutdown(ctx);
    }

    void SkinnedMeshPass::SetupFrame(
        VkExtent2D                   extent,
        VkDescriptorSet              globalUboSet,
        std::vector<SkinnedDrawCall> drawCalls)
    {
        m_extent       = extent;
        m_globalUboSet = globalUboSet;
        m_drawCalls    = std::move(drawCalls);
    }

    void SkinnedMeshPass::Execute(VkCommandBuffer cmd)
    {
        if (m_drawCalls.empty()) return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.Handle());

        VkViewport viewport{};
        viewport.x = 0.0f; viewport.y = 0.0f;
        viewport.width  = static_cast<f32>(m_extent.width);
        viewport.height = static_cast<f32>(m_extent.height);
        viewport.minDepth = 0.0f; viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = m_extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // Bind global UBO (set 0)
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipeline.Layout(), 0, 1, &m_globalUboSet, 0, nullptr);

        for (const SkinnedDrawCall& dc : m_drawCalls)
        {
            if (!dc.mesh || !dc.material || !dc.boneMatrices) continue;

            // Upload bone matrices to GPU storage buffer
            const usize boneCount = std::min(dc.boneMatrices->size(), static_cast<usize>(kMaxBones));
            const usize uploadSize = boneCount * sizeof(Mat4);
            if (uploadSize > 0)
            {
                void* dst = m_bonePaletteBuffer.Map(const_cast<VulkanContext&>(*static_cast<const VulkanContext*>(nullptr)));
                // NOTE: VulkanBuffer::CopyData is the proper API
                // We use the pre-mapped persistent pointer (HOST_COHERENT buffer)
                if (m_bonePaletteBuffer.Mapped())
                {
                    std::memcpy(m_bonePaletteBuffer.Mapped(),
                                dc.boneMatrices->data(),
                                uploadSize);
                }
            }

            // Bind material textures (set 1)
            VkDescriptorSet matSet = dc.material->GetDescriptorSet();
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline.Layout(), 1, 1, &matSet, 0, nullptr);

            // Bind bone palette SSBO (set 2)
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipeline.Layout(), 2, 1, &m_bonePaletteSet, 0, nullptr);

            // Push model matrix + color
            SkinnedPushConstants pc{};
            pc.modelMatrix = dc.modelMatrix;
            pc.color       = dc.color;
            vkCmdPushConstants(cmd, m_pipeline.Layout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(SkinnedPushConstants), &pc);

            // Bind vertex + index buffers
            VkBuffer vb = dc.mesh->GetVertexBuffer();
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
            vkCmdBindIndexBuffer(cmd, dc.mesh->GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

            vkCmdDrawIndexed(cmd, dc.mesh->GetIndexCount(), 1, 0, 0, 0);
        }
    }
}
