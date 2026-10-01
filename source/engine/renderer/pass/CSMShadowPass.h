// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// CSMShadowPass.h - Lacrima Engine Step 4: Cascaded Shadow Maps
//
// Renders the scene into a Vulkan 2D array depth image (one layer per cascade)
// from the directional light perspective.
//
// Descriptor set layout (shadow pass only):
//   Set 0 (binding 0): ShadowUBO  { mat4 lightSpaceMatrices[4] }
//
// Push constants per draw:
//   mat4  modelMatrix     (64 bytes)
//   uint  cascadeIndex    (4 bytes)
//   float _pad[3]         (12 bytes)
//   Total: 80 bytes
//
// Usage:
//   1. Call Initialize() at renderer startup
//   2. Each frame:
//      a. CSMFrustumSplitter::ComputeSplits() to get cascade ranges
//      b. BuildLightSpaceMatrices() to get per-cascade view-proj matrices
//      c. UploadShadowUBO()
//      d. RecordShadowCommands() (runs BEFORE the main offscreen pass)
//   3. Pass GetShadowMapArrayView() + cascade split depths to GlobalUniforms
//      so static_mesh.frag / skinned_mesh.frag can sample PCF shadows.
// ---------------------------------------------------------------------------

#include "core/math/Math.h"
#include "core/platform/Types.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "renderer/vulkan/VulkanPipeline.h"
#include "renderer/vulkan/VulkanShader.h"
#include "renderer/resource/GPUMesh.h"
#include "runtime/SimulationSnapshot.h"

#include <vulkan/vulkan.h>
#include <array>
#include <vector>

namespace lacrima::renderer
{
    // Number of shadow cascade splits.
    // Must match kMaxCSMCascades in VulkanGlobalUniforms.h.
    static constexpr u32 kCSMCascadeCount = 4;

    // Resolution of each cascade layer (square).
    static constexpr u32 kCSMResolution = 2048;

    // ---------------------------------------------------------------------------
    // CSMFrustumSplitter
    //
    // Computes camera-frustum cascade split distances using a logarithmic /
    // practical split scheme (blend of log and uniform, lambda = 0.75).
    // Results are in view-space Z (negative = in front of camera).
    // ---------------------------------------------------------------------------
    class CSMFrustumSplitter
    {
    public:
        struct Splits
        {
            // cascadeNear[i] / cascadeFar[i]: view-space Z interval for cascade i
            float cascadeNear[kCSMCascadeCount];
            float cascadeFar[kCSMCascadeCount];
        };

        // nearZ / farZ: camera clip planes (positive values, e.g. 0.1f / 500.0f)
        // lambda: 0 = uniform split, 1 = pure log split. 0.75f is a common default.
        static Splits ComputeSplits(float nearZ, float farZ, float lambda = 0.75f);

        // Computes the orthographic light-space matrix for one cascade.
        // frustumCorners: 8 world-space corners of the cascade sub-frustum.
        static Mat4 BuildLightSpaceMatrix(
            const Vec3 lightDir,          // normalised direction TO light source
            const Vec3 frustumCorners[8], // world-space frustum corners
            float texelSnapSize           // snap to texels to reduce shadow shimmering (pass kCSMResolution)
        );

        // Extracts the 8 world-space corners of the view frustum slice
        // between nearZ and farZ.
        static void ExtractFrustumCorners(
            const Mat4& invViewProj, // inverse(proj * view) of the main camera
            float nearZ, float farZ,
            Vec3 outCorners[8]
        );
    };

    // ---------------------------------------------------------------------------
    // CSMShadowPass
    //
    // Owns the shadow map image array (VkImage, VkImageView[4], VkFramebuffer[4])
    // and the depth-only graphics pipeline used to populate it each frame.
    // ---------------------------------------------------------------------------
    class CSMShadowPass
    {
    public:
        CSMShadowPass();
        ~CSMShadowPass();

        // Non-copyable
        CSMShadowPass(const CSMShadowPass&)            = delete;
        CSMShadowPass& operator=(const CSMShadowPass&) = delete;

        bool Initialize(VulkanContext& ctx);
        void Shutdown(VulkanContext& ctx);

        // Upload the per-cascade light-space matrices to the shadow UBO.
        // Call once per frame before RecordShadowCommands().
        void UploadLightSpaceMatrices(VulkanContext& ctx,
                                      const Mat4 lightSpaceMatrices[kCSMCascadeCount]);

        // Records commands to populate all 4 cascade shadow maps.
        // Must be called BEFORE the main offscreen colour pass begins.
        // proxies: list of renderable entities from the SimSnapshot.
        void RecordShadowCommands(VkCommandBuffer cmd,
                                  const std::vector<RenderProxy>& proxies,
                                  const GPUMesh& mesh);

        // Returns the combined 2D array view (all 4 layers) for binding in the
        // main fragment shader as sampler2DArrayShadow.
        VkImageView GetShadowMapArrayView() const { return m_shadowArrayView; }

        // Returns the sampler configured for PCF comparison (LESS op).
        VkSampler GetShadowSampler() const { return m_shadowSampler; }

        bool IsInitialized() const { return m_depthImage != VK_NULL_HANDLE; }

    private:
        // --- Vulkan resource creation helpers ---
        bool CreateDepthImage(VulkanContext& ctx);
        bool CreatePerCascadeImageViews(VulkanContext& ctx);
        bool CreateRenderPass(VulkanContext& ctx);
        bool CreateFramebuffers(VulkanContext& ctx);
        bool CreateShadowSampler(VulkanContext& ctx);
        bool CreateShadowUBOLayout(VulkanContext& ctx);
        bool CreateShadowUBOSet(VulkanContext& ctx);
        bool CreatePipeline(VulkanContext& ctx);

        void TransitionDepthImageForWrite(VkCommandBuffer cmd);
        void TransitionDepthImageForRead(VkCommandBuffer cmd);

        // --- Depth image (2D array with kCSMCascadeCount layers) ---
        VkImage        m_depthImage      = VK_NULL_HANDLE;
        VkDeviceMemory m_depthMemory     = VK_NULL_HANDLE;
        VkImageView    m_shadowArrayView = VK_NULL_HANDLE; // all layers, for shader sampling
        std::array<VkImageView,    kCSMCascadeCount> m_cascadeViews{};    // one per layer (for framebuffer)
        std::array<VkFramebuffer,  kCSMCascadeCount> m_cascadeFramebuffers{};

        // --- Render pass (depth-only, no colour attachments) ---
        VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;

        // --- Sampler with comparison op for PCF ---
        VkSampler m_shadowSampler = VK_NULL_HANDLE;

        // --- Shadow UBO (light-space matrices) ---
        struct ShadowUBOData
        {
            float lightSpaceMatrices[kCSMCascadeCount][16]; // 4 x mat4
        };
        VulkanBuffer          m_shadowUBO;
        VkDescriptorSetLayout m_shadowUBOLayout = VK_NULL_HANDLE;
        VkDescriptorPool      m_shadowDescPool  = VK_NULL_HANDLE;
        VkDescriptorSet       m_shadowUBOSet    = VK_NULL_HANDLE;

        // --- Graphics pipeline (depth-only) ---
        VulkanShader   m_vertShader;
        VulkanPipeline m_pipeline;
    };
}
