// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/pass/CSMShadowPass.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/resource/GPUMesh.h"
#include "core/logging/Logger.h"
#include "core/filesystem/FileSystem.h"
#include "core/math/Math.h"

#include <cmath>
#include <cstring>
#include <array>
#include <climits>

namespace lacrima::renderer
{
    // =========================================================================
    // CSMFrustumSplitter
    // =========================================================================

    CSMFrustumSplitter::Splits CSMFrustumSplitter::ComputeSplits(float nearZ, float farZ, float lambda)
    {
        Splits splits{};

        // Practical split scheme: blend of log and uniform
        // Reference: GPU Gems 3 Chapter 10
        const float range = farZ - nearZ;
        const float ratio = farZ / nearZ;

        for (u32 i = 0; i < kCSMCascadeCount; ++i)
        {
            const float p       = (i + 1) / static_cast<float>(kCSMCascadeCount);
            const float logSplit = nearZ * std::pow(ratio, p);
            const float uniSplit = nearZ + range * p;
            const float d = lambda * (logSplit - uniSplit) + uniSplit;

            splits.cascadeFar[i] = d;
            splits.cascadeNear[i] = (i == 0) ? nearZ : splits.cascadeFar[i - 1];
        }
        return splits;
    }

    void CSMFrustumSplitter::ExtractFrustumCorners(
        const Mat4& invViewProj,
        float nearZ, float farZ,
        Vec3 outCorners[8])
    {
        // NDC corners at near and far planes (Vulkan depth [0,1])
        // Order: near (TL, TR, BL, BR), far (TL, TR, BL, BR)
        const float ndcNear = 0.0f; // Vulkan: near = 0
        const float ndcFar  = 1.0f; // Vulkan: far  = 1
        (void)nearZ; (void)farZ;

        const Vec4 ndcCorners[8] = {
            { -1.0f,  1.0f, ndcNear, 1.0f }, // near TL
            {  1.0f,  1.0f, ndcNear, 1.0f }, // near TR
            { -1.0f, -1.0f, ndcNear, 1.0f }, // near BL
            {  1.0f, -1.0f, ndcNear, 1.0f }, // near BR
            { -1.0f,  1.0f, ndcFar,  1.0f }, // far  TL
            {  1.0f,  1.0f, ndcFar,  1.0f }, // far  TR
            { -1.0f, -1.0f, ndcFar,  1.0f }, // far  BL
            {  1.0f, -1.0f, ndcFar,  1.0f }, // far  BR
        };

        for (u32 i = 0; i < 8; ++i)
        {
            // Transform NDC corner -> world space via inverse(VP)
            const Vec4& n = ndcCorners[i];
            // Manual mat4 * vec4 using column-major m[16]
            const float* m = invViewProj.m;
            float wx = m[0]*n.x + m[4]*n.y + m[8]*n.z  + m[12]*n.w;
            float wy = m[1]*n.x + m[5]*n.y + m[9]*n.z  + m[13]*n.w;
            float wz = m[2]*n.x + m[6]*n.y + m[10]*n.z + m[14]*n.w;
            float ww = m[3]*n.x + m[7]*n.y + m[11]*n.z + m[15]*n.w;

            if (std::abs(ww) > 1e-8f)
            {
                wx /= ww; wy /= ww; wz /= ww;
            }
            outCorners[i] = Vec3(wx, wy, wz);
        }
    }

    Mat4 CSMFrustumSplitter::BuildLightSpaceMatrix(
        const Vec3 lightDir,
        const Vec3 frustumCorners[8],
        float texelSnapSize)
    {
        // Compute frustum centroid
        Vec3 center(0.0f, 0.0f, 0.0f);
        for (u32 i = 0; i < 8; ++i)
        {
            center.x += frustumCorners[i].x;
            center.y += frustumCorners[i].y;
            center.z += frustumCorners[i].z;
        }
        center.x /= 8.0f;
        center.y /= 8.0f;
        center.z /= 8.0f;

        // Light view matrix: look from center + lightDir toward center
        const Vec3 worldUp = (std::abs(lightDir.y) < 0.99f) ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
        const Mat4 lightView = Mat4::LookAt(center + lightDir, center, worldUp);

        // Transform all corners into light space to compute AABB
        float minX =  1e30f, maxX = -1e30f;
        float minY =  1e30f, maxY = -1e30f;
        float minZ =  1e30f, maxZ = -1e30f;

        const float* lv = lightView.m;
        for (u32 i = 0; i < 8; ++i)
        {
            const Vec3& wc = frustumCorners[i];
            // Transform by lightView (column-major)
            float lx = lv[0]*wc.x + lv[4]*wc.y + lv[8]*wc.z  + lv[12];
            float ly = lv[1]*wc.x + lv[5]*wc.y + lv[9]*wc.z  + lv[13];
            float lz = lv[2]*wc.x + lv[6]*wc.y + lv[10]*wc.z + lv[14];

            if (lx < minX) minX = lx; if (lx > maxX) maxX = lx;
            if (ly < minY) minY = ly; if (ly > maxY) maxY = ly;
            if (lz < minZ) minZ = lz; if (lz > maxZ) maxZ = lz;
        }

        // Extend Z to capture casters behind the frustum
        const float zMult = 10.0f;
        if (minZ < 0.0f) minZ *= zMult; else minZ /= zMult;
        if (maxZ < 0.0f) maxZ /= zMult; else maxZ *= zMult;

        // Snap to texel grid to reduce shadow shimmering
        if (texelSnapSize > 0.0f)
        {
            const float worldUnitsPerTexel = (maxX - minX) / texelSnapSize;
            auto snap = [&](float v) {
                return std::floor(v / worldUnitsPerTexel) * worldUnitsPerTexel;
            };
            minX = snap(minX); maxX = snap(maxX);
            minY = snap(minY); maxY = snap(maxY);
        }

        const Mat4 lightProj = Mat4::Orthographic(minX, maxX, minY, maxY, minZ, maxZ);
        return lightProj * lightView;
    }

    // =========================================================================
    // CSMShadowPass
    // =========================================================================

    CSMShadowPass::CSMShadowPass() = default;
    CSMShadowPass::~CSMShadowPass() = default;

    bool CSMShadowPass::Initialize(VulkanContext& ctx)
    {
        if (!CreateDepthImage(ctx))          return false;
        if (!CreatePerCascadeImageViews(ctx)) return false;
        if (!CreateRenderPass(ctx))           return false;
        if (!CreateFramebuffers(ctx))         return false;
        if (!CreateShadowSampler(ctx))        return false;
        if (!CreateShadowUBOLayout(ctx))      return false;
        if (!CreateShadowUBOSet(ctx))         return false;
        if (!CreatePipeline(ctx))             return false;

        LACRIMA_LOG_INFO(LogCategory::Renderer, "CSMShadowPass: initialized ({} cascades, {}x{} each)",
                         kCSMCascadeCount, kCSMResolution, kCSMResolution);
        return true;
    }

    void CSMShadowPass::Shutdown(VulkanContext& ctx)
    {
        VkDevice dev = ctx.Device();

        m_pipeline.Shutdown(ctx);
        m_vertShader.Shutdown(ctx);

        if (m_shadowDescPool   != VK_NULL_HANDLE) { vkDestroyDescriptorPool(dev,      m_shadowDescPool,   nullptr); m_shadowDescPool   = VK_NULL_HANDLE; }
        if (m_shadowUBOLayout  != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(dev, m_shadowUBOLayout,  nullptr); m_shadowUBOLayout  = VK_NULL_HANDLE; }
        m_shadowUBO.Shutdown(ctx);

        if (m_shadowSampler    != VK_NULL_HANDLE) { vkDestroySampler(dev,             m_shadowSampler,    nullptr); m_shadowSampler    = VK_NULL_HANDLE; }

        for (u32 i = 0; i < kCSMCascadeCount; ++i)
        {
            if (m_cascadeFramebuffers[i] != VK_NULL_HANDLE) { vkDestroyFramebuffer(dev,  m_cascadeFramebuffers[i], nullptr); m_cascadeFramebuffers[i] = VK_NULL_HANDLE; }
            if (m_cascadeViews[i]        != VK_NULL_HANDLE) { vkDestroyImageView(dev,    m_cascadeViews[i],        nullptr); m_cascadeViews[i]        = VK_NULL_HANDLE; }
        }

        if (m_shadowRenderPass != VK_NULL_HANDLE) { vkDestroyRenderPass(dev,      m_shadowRenderPass, nullptr); m_shadowRenderPass = VK_NULL_HANDLE; }
        if (m_shadowArrayView  != VK_NULL_HANDLE) { vkDestroyImageView(dev,       m_shadowArrayView,  nullptr); m_shadowArrayView  = VK_NULL_HANDLE; }
        if (m_depthImage       != VK_NULL_HANDLE) { vkDestroyImage(dev,           m_depthImage,       nullptr); m_depthImage       = VK_NULL_HANDLE; }
        if (m_depthMemory      != VK_NULL_HANDLE) { vkFreeMemory(dev,             m_depthMemory,      nullptr); m_depthMemory      = VK_NULL_HANDLE; }
    }

    // -------------------------------------------------------------------------
    bool CSMShadowPass::CreateDepthImage(VulkanContext& ctx)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.format        = VK_FORMAT_D32_SFLOAT;
        imageInfo.extent        = { kCSMResolution, kCSMResolution, 1 };
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = kCSMCascadeCount;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(ctx.Device(), &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "CSMShadowPass: failed to create depth image array");
            return false;
        }

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(ctx.Device(), m_depthImage, &memReq);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memReq.size;
        allocInfo.memoryTypeIndex = ctx.FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(ctx.Device(), &allocInfo, nullptr, &m_depthMemory) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "CSMShadowPass: failed to allocate depth image memory");
            return false;
        }
        vkBindImageMemory(ctx.Device(), m_depthImage, m_depthMemory, 0);
        return true;
    }

    bool CSMShadowPass::CreatePerCascadeImageViews(VulkanContext& ctx)
    {
        // Full array view (for fragment shader sampling)
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = m_depthImage;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.format                          = VK_FORMAT_D32_SFLOAT;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = kCSMCascadeCount;

        if (vkCreateImageView(ctx.Device(), &viewInfo, nullptr, &m_shadowArrayView) != VK_SUCCESS)
            return false;

        // Per-cascade single-layer views (for framebuffers)
        for (u32 i = 0; i < kCSMCascadeCount; ++i)
        {
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.subresourceRange.baseArrayLayer = i;
            viewInfo.subresourceRange.layerCount     = 1;

            if (vkCreateImageView(ctx.Device(), &viewInfo, nullptr, &m_cascadeViews[i]) != VK_SUCCESS)
            {
                LACRIMA_LOG_ERROR(LogCategory::Renderer, "CSMShadowPass: failed to create cascade view {}", i);
                return false;
            }
        }
        return true;
    }

    bool CSMShadowPass::CreateRenderPass(VulkanContext& ctx)
    {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 0;
        depthRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthRef;

        // Subpass dependency: ensure depth writes finish before fragment stage reads
        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass      = 0;
        dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass      = 0;
        dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments    = &depthAttachment;
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = static_cast<u32>(dependencies.size());
        rpInfo.pDependencies   = dependencies.data();

        return vkCreateRenderPass(ctx.Device(), &rpInfo, nullptr, &m_shadowRenderPass) == VK_SUCCESS;
    }

    bool CSMShadowPass::CreateFramebuffers(VulkanContext& ctx)
    {
        for (u32 i = 0; i < kCSMCascadeCount; ++i)
        {
            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass      = m_shadowRenderPass;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments    = &m_cascadeViews[i];
            fbInfo.width           = kCSMResolution;
            fbInfo.height          = kCSMResolution;
            fbInfo.layers          = 1;

            if (vkCreateFramebuffer(ctx.Device(), &fbInfo, nullptr, &m_cascadeFramebuffers[i]) != VK_SUCCESS)
            {
                LACRIMA_LOG_ERROR(LogCategory::Renderer, "CSMShadowPass: failed to create framebuffer for cascade {}", i);
                return false;
            }
        }
        return true;
    }

    bool CSMShadowPass::CreateShadowSampler(VulkanContext& ctx)
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter        = VK_FILTER_LINEAR;
        samplerInfo.minFilter        = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE; // outside shadow = no shadow
        samplerInfo.compareEnable    = VK_TRUE;
        samplerInfo.compareOp        = VK_COMPARE_OP_LESS;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy    = 1.0f;

        return vkCreateSampler(ctx.Device(), &samplerInfo, nullptr, &m_shadowSampler) == VK_SUCCESS;
    }

    bool CSMShadowPass::CreateShadowUBOLayout(VulkanContext& ctx)
    {
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding         = 0;
        uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &uboBinding;

        return vkCreateDescriptorSetLayout(ctx.Device(), &layoutInfo, nullptr, &m_shadowUBOLayout) == VK_SUCCESS;
    }

    bool CSMShadowPass::CreateShadowUBOSet(VulkanContext& ctx)
    {
        // Allocate descriptor pool
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;

        if (vkCreateDescriptorPool(ctx.Device(), &poolInfo, nullptr, &m_shadowDescPool) != VK_SUCCESS)
            return false;

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_shadowDescPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_shadowUBOLayout;

        if (vkAllocateDescriptorSets(ctx.Device(), &allocInfo, &m_shadowUBOSet) != VK_SUCCESS)
            return false;

        // Create and bind shadow UBO (host-coherent for per-frame CPU writes)
        if (!m_shadowUBO.Initialize(ctx,
                                    sizeof(ShadowUBOData),
                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            return false;

        m_shadowUBO.Map(ctx);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_shadowUBO.Handle();
        bufferInfo.offset = 0;
        bufferInfo.range  = sizeof(ShadowUBOData);

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = m_shadowUBOSet;
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo     = &bufferInfo;

        vkUpdateDescriptorSets(ctx.Device(), 1, &write, 0, nullptr);
        return true;
    }

    bool CSMShadowPass::CreatePipeline(VulkanContext& ctx)
    {
        // Load depth-only vertex shader
        if (!m_vertShader.InitializeFromCookedFile(ctx, FileSystem::GetEngineAssetDir() + "/shadow_depth_vert.asset"))
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "CSMShadowPass: failed to load shadow_depth_vert.asset");
            return false;
        }

        // Push constants: model matrix (64 bytes) + cascade index (4) + 12 pad
        VkPushConstantRange pushConstant{};
        pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstant.offset     = 0;
        pushConstant.size       = sizeof(float) * 16 + sizeof(uint32_t) + sizeof(float) * 3; // 80 bytes

        // Vertex input: same layout as static mesh (pos/normal/uv/tangent)
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(Vertex); // pos(12) + normal(12) + uv(8) + tangent(16) = 48
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attrs[4]{};
        attrs[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex, position) };
        attrs[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT,    offsetof(Vertex, normal)   };
        attrs[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT,       offsetof(Vertex, texCoord) };
        attrs[3] = { 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, tangent)  };

        VulkanPipeline::Config config{};
        config.renderPass                = m_shadowRenderPass;
        config.vertShader                = &m_vertShader;
        config.fragShader                = nullptr;        // depth-only: no fragment shader
        config.pDescriptorSetLayouts     = &m_shadowUBOLayout;
        config.descriptorSetLayoutCount  = 1;
        config.pPushConstantRanges       = &pushConstant;
        config.pushConstantRangeCount    = 1;
        config.enableDepthTest           = true;
        config.enableAlphaBlend          = false;
        config.pVertexBindings           = &binding;
        config.vertexBindingCount        = 1;
        config.pVertexAttributes         = attrs;
        config.vertexAttributeCount      = 4;

        return m_pipeline.Initialize(ctx, config);
    }

    // =========================================================================
    // Per-frame update and recording
    // =========================================================================

    void CSMShadowPass::UploadLightSpaceMatrices(VulkanContext& ctx,
                                                  const Mat4 lightSpaceMatrices[kCSMCascadeCount])
    {
        (void)ctx;
        if (!m_shadowUBO.Mapped()) return;

        ShadowUBOData data{};
        for (u32 i = 0; i < kCSMCascadeCount; ++i)
            std::memcpy(data.lightSpaceMatrices[i], lightSpaceMatrices[i].m, 16 * sizeof(float));

        std::memcpy(m_shadowUBO.Mapped(), &data, sizeof(ShadowUBOData));
        // HOST_COHERENT: no flush required
    }

        void CSMShadowPass::RecordShadowCommands(VkCommandBuffer cmd,
                                              const std::vector<RenderProxy>& proxies,
                                              const GPUMesh& mesh)
    {
        if (!m_pipeline.IsInitialized()) return;
        if (mesh.GetVertexBuffer() == VK_NULL_HANDLE) return;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.Handle());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_pipeline.Layout(), 0, 1, &m_shadowUBOSet, 0, nullptr);

        VkBuffer     vbufs[] = { mesh.GetVertexBuffer() };
        VkDeviceSize offs[]  = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offs);
        vkCmdBindIndexBuffer(cmd, mesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        VkViewport vp{};
        vp.x        = 0.0f; vp.y        = 0.0f;
        vp.width    = static_cast<float>(kCSMResolution);
        vp.height   = static_cast<float>(kCSMResolution);
        vp.minDepth = 0.0f; vp.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = { kCSMResolution, kCSMResolution };

        const u32 indexCount = mesh.GetIndexCount();

        for (u32 cascade = 0; cascade < kCSMCascadeCount; ++cascade)
        {
            VkRenderPassBeginInfo rpBegin{};
            rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderPass        = m_shadowRenderPass;
            rpBegin.framebuffer       = m_cascadeFramebuffers[cascade];
            rpBegin.renderArea.offset = {0, 0};
            rpBegin.renderArea.extent = { kCSMResolution, kCSMResolution };

            VkClearValue clearValue{};
            clearValue.depthStencil = { 1.0f, 0 };
            rpBegin.clearValueCount = 1;
            rpBegin.pClearValues    = &clearValue;

            vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdSetViewport(cmd, 0, 1, &vp);
            vkCmdSetScissor(cmd, 0, 1, &scissor);

            for (const auto& proxy : proxies)
            {
                // Push constants: model matrix + cascade index
                struct ShadowPC {
                    float model[16];
                    uint32_t cascadeIndex;
                    float _pad[3];
                };

                ShadowPC pc{};
                // Build simple TRS model matrix for proxy
                Mat4 modelMat = Mat4::Translation(Vec3(proxy.positionX, proxy.positionY, proxy.positionZ));
                const float s = 0.01f * proxy.scaleX;
                modelMat.m[0]  = s; modelMat.m[5]  = s; modelMat.m[10] = s;
                std::memcpy(pc.model, modelMat.m, 16 * sizeof(float));
                pc.cascadeIndex = cascade;

                vkCmdPushConstants(cmd, m_pipeline.Layout(),
                                   VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPC), &pc);
                vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
            }

            vkCmdEndRenderPass(cmd);
        }
    }
}
