#pragma once

#include "renderer/IRenderer.h"
#include "renderer/ImGuiLayer.h"
#include "renderer/sdl/SDLWindow.h"
#include "renderer/sdl/SDLInputMapper.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanSwapchain.h"
#include "renderer/vulkan/VulkanRenderPass.h"
#include "renderer/vulkan/VulkanCommandPool.h"
#include "renderer/vulkan/VulkanSync.h"
#include "renderer/vulkan/VulkanMemoryAllocator.h"
#include "renderer/vulkan/VulkanOffscreenTarget.h"

#include <vulkan/vulkan.h>
#include <vector>
#include "renderer/Camera.h"
#include "renderer/vulkan/VulkanDescriptorPool.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "renderer/vulkan/VulkanMaterial.h"
#include "renderer/resource/GPUTexture.h"
#include "renderer/resource/GPUMesh.h"
#include "renderer/vulkan/VulkanGlobalUniforms.h"
#include "renderer/pass/SpriteRenderPass.h"
#include "renderer/pass/MeshRenderPass.h"
#include "renderer/pass/RaycastRenderPass.h"
#include "renderer/pass/SkinnedMeshPass.h"
#include "renderer/pass/CSMShadowPass.h"

namespace lacrima
{
    class Application;

    class VulkanRenderer : public IRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer() override = default;

        void SetApplication(Application* app) { m_app = app; }
        void SetWindowTitle(const char* title) { m_windowTitle = title; }
        void SetSpriteMaterial(renderer::VulkanMaterial* material) { m_projectMaterial = material; }
        void SetProjectTextureAsset(const char* path, bool pixelPerfect = false)
        {
            m_projectTexturePath = path ? path : "";
            m_projectTexturePixelPerfect = pixelPerfect;
        }
        void SetRaycastAtlasRows(u32 rows) { m_raycastPass.SetAtlasRowCount(rows); }
        renderer::VulkanContext& GetContext() { return m_context; }

        // --- Offscreen Viewport Target (for Editor 3D Viewport) ------------
        void ResizeOffscreen(u32 width, u32 height)
        {
            if (width > 0 && height > 0 && (width != m_offscreenTarget.Width() || height != m_offscreenTarget.Height()))
            {
                m_pendingOffscreenResize = true;
                m_pendingOffscreenWidth = width;
                m_pendingOffscreenHeight = height;
            }
        }
        VkDescriptorSet GetOffscreenDescriptorSet() const { return m_offscreenTarget.DescriptorSet(); }

        // --- Custom Camera Override (e.g. from EditorCamera) ----------------
        void SetCustomCamera(const Vec3& pos, const Mat4& view, const Mat4& proj)
        {
            m_useCustomCamera = true;
            m_customCameraPos = pos;
            m_customView = view;
            m_customProj = proj;
        }

        // --- IRenderer interface implementation ----------------------------

        bool Initialize() override;
        void Shutdown() override;
        bool Render(const SimSnapshot& snapshot) override;

        f32 TargetFramesPerSecond() const override { return 60.0f; }

        void SetDropCallback(DropCallback cb) override { m_dropCallback = std::move(cb); }

        using EditorConstructCallback = std::function<void()>;
        void SetEditorConstructCallback(EditorConstructCallback cb) { m_editorConstructCallback = std::move(cb); }

    private:
        void RecreateSwapchain();

        DropCallback m_dropCallback;
        EditorConstructCallback m_editorConstructCallback;

        Application* m_app = nullptr;

        renderer::SDLWindow          m_window;
        renderer::VulkanContext      m_context;
        renderer::VulkanSwapchain    m_swapchain;
        renderer::VulkanRenderPass   m_renderPass;
        renderer::VulkanCommandPool  m_commandPool;
        renderer::VulkanSync         m_sync;
        renderer::VulkanMemoryAllocator m_allocator;
        renderer::ImGuiLayer         m_imguiLayer;

        renderer::VulkanOffscreenTarget m_offscreenTarget;
        bool                         m_pendingOffscreenResize = false;
        u32                          m_pendingOffscreenWidth  = 0;
        u32                          m_pendingOffscreenHeight = 0;

        VkSurfaceKHR m_surface           = VK_NULL_HANDLE;
        u32          m_currentFrameIndex  = 0;
        bool         m_resized            = false;
        const char*  m_windowTitle        = "Lacrima Engine";
        u32          m_width              = 1280;
        u32          m_height             = 720;

        SDLInputMapper               m_inputMapper;

        // Global systems
        renderer::Camera               m_camera;
        bool                           m_useCustomCamera = false;
        Vec3                           m_customCameraPos{0.0f, 0.0f, 0.0f};
        Mat4                           m_customView = Mat4::Identity();
        Mat4                           m_customProj = Mat4::Identity();

        renderer::VulkanDescriptorPool m_descriptorPool;
        renderer::VulkanBuffer         m_globalUBO;
        VkDescriptorSetLayout          m_globalUBOLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout          m_materialLayout = VK_NULL_HANDLE;
        VkDescriptorSet                m_globalUBOSet = VK_NULL_HANDLE;

        renderer::SpriteRenderPass     m_spritePass;
        renderer::MeshRenderPass       m_meshPass;
        renderer::RaycastRenderPass    m_raycastPass;
        renderer::SkinnedMeshPass      m_skinnedMeshPass;
        renderer::CSMShadowPass        m_csmPass;

        // Test assets
        renderer::GPUTexture           m_testTexture;
        renderer::GPUTexture           m_defaultAlbedo;
        renderer::GPUTexture           m_defaultNormal;
        renderer::GPUTexture           m_defaultMR;
        renderer::GPUTexture           m_defaultAO;
        renderer::GPUTexture           m_defaultEmissive;
        renderer::GPUMesh              m_playerMesh;
        std::vector<RenderProxy>       m_meshProxies;
        renderer::VulkanMaterial       m_testSpriteMaterial;
        renderer::VulkanMaterial*      m_projectMaterial = nullptr;
        std::string                    m_projectTexturePath;
        bool                           m_projectTexturePixelPerfect = false;
    };
}
