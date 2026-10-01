// Copyright Neofilisoft. All Rights Reserved.
#pragma once

// ---------------------------------------------------------------------------
// SkinnedMeshPass.h - Lacrima Engine Vulkan Render Pass (Step 3)
//
// Renders GPU-skinned skeletal meshes using skinned_mesh.vert/.frag.
//
// Descriptor set layout:
//   Set 0 (binding 0): GlobalUniforms UBO  (view/proj/lighting)
//   Set 1 (binding 0-4): PBR material textures (albedo/normal/mr/ao/emissive)
//   Set 2 (binding 0): BonePalette SSBO (mat4 bones[128])
//
// Per-draw call:
//   Push constants: model matrix + tint color
//   Vertex buffer: SkinnedVertex (pos/normal/uv/tangent/boneIndices/boneWeights)
//   Index buffer:  u32 indices from GPUMesh
//
// Usage:
//   1. Initialize() at renderer startup
//   2. Each frame: SetupFrame() once, then Execute() inside a render pass
// ---------------------------------------------------------------------------

#include "renderer/graph/RenderGraph.h"
#include "renderer/vulkan/VulkanPipeline.h"
#include "renderer/vulkan/VulkanShader.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "runtime/SimulationSnapshot.h"
#include "core/math/Math.h"
#include <vector>

namespace lacrima::renderer
{
    class VulkanContext;
    class VulkanMaterial;
    class GPUMesh;

    // One draw request submitted by SkinnedMeshPass per entity
    struct SkinnedDrawCall
    {
        const GPUMesh*          mesh            = nullptr;
        const VulkanMaterial*   material        = nullptr;
        const std::vector<Mat4>* boneMatrices   = nullptr; // pointer into SkeletalMeshComponent
        Mat4                    modelMatrix      = Mat4::Identity();
        Vec4                    color            = {1.0f, 1.0f, 1.0f, 1.0f};
    };

    class SkinnedMeshPass : public RenderPass
    {
    public:
        static constexpr u32 kMaxBones = 128;

        SkinnedMeshPass();
        ~SkinnedMeshPass() override = default;

        bool Initialize(
            VulkanContext&           ctx,
            VkRenderPass             renderPass,
            VkDescriptorSetLayout    uboLayout,
            VkDescriptorSetLayout    materialLayout);

        void Shutdown(VulkanContext& ctx);

        // Submit draw calls for this frame; call before Execute()
        void SetupFrame(
            VkExtent2D              extent,
            VkDescriptorSet         globalUboSet,
            std::vector<SkinnedDrawCall> drawCalls);

        void Execute(VkCommandBuffer cmd) override;

        bool IsInitialized() const { return m_pipeline.IsInitialized(); }

    private:
        bool CreateBonePaletteLayout(VulkanContext& ctx);

        VulkanShader    m_vertShader;
        VulkanShader    m_fragShader;
        VulkanPipeline  m_pipeline;

        // Descriptor pool + layout for bone palette storage buffer (Set 2)
        VkDescriptorSetLayout m_bonePaletteLayout = VK_NULL_HANDLE;
        VkDescriptorPool      m_boneDescPool      = VK_NULL_HANDLE;

        // Per-frame GPU storage buffer: enough for kMaxBones * sizeof(Mat4)
        VulkanBuffer    m_bonePaletteBuffer;
        VkDescriptorSet m_bonePaletteSet = VK_NULL_HANDLE;

        // Frame data
        VkExtent2D                   m_extent{};
        VkDescriptorSet              m_globalUboSet = VK_NULL_HANDLE;
        std::vector<SkinnedDrawCall> m_drawCalls;
    };
}
