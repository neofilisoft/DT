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

#include <vulkan/vulkan.h>
#include "renderer/Camera.h"
#include "renderer/vulkan/VulkanDescriptorPool.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "renderer/vulkan/VulkanMaterial.h"
#include "renderer/resource/GpuTexture.h"
#include "renderer/resource/GpuMesh.h"
#include "renderer/vulkan/VulkanGlobalUniforms.h"
#include "renderer/pass/SpriteRenderPass.h"
#include "renderer/pass/MeshRenderPass.h"
// ---------------------------------------------------------------------------
// VulkanRenderer.h
//
// The Vulkan-backed rendering backend for DTEngine.
// Owns the complete rendering lifecycle, translating the SimSnapshot into
// colored agent quads and drawing an ImGui HUD overlay.
//
// Life-cycle (drives the Render thread):
//   1. Main thread instantiates VulkanRenderer, sets it in Application.
//   2. Render thread starts, calls Initialize() to bring up SDL3 window,
//      Vulkan instance/device/swapchain, pipeline, and ImGui.
//   3. Render thread loops, calling Render(SimSnapshot) every 16ms (60 FPS).
//   4. Render thread stops, calls Shutdown() to destroy everything.
//
// Synchronization:
//   - vkWaitForFences/vkResetFences coordinates CPU-GPU frames-in-flight.
//   - Acquire/Present semaphores coordinate GPU-swapchain presentation.
// ---------------------------------------------------------------------------

namespace dt
{
    class Application;

    class VulkanRenderer : public IRenderer
    {
    public:
        VulkanRenderer();
        ~VulkanRenderer() override = default;

        // Custom pointer linkage to the parent application.
        // Wired up in main.cpp immediately after application creation.
        void SetApplication(Application* app) { m_app = app; }
        renderer::VulkanContext& GetContext() { return m_context; }

        // --- IRenderer interface implementation ----------------------------

        bool Initialize() override;
        void Shutdown() override;
        void Render(const SimSnapshot& snapshot) override;

        f32 TargetFramesPerSecond() const override { return 60.0f; }

    private:
        void RecreateSwapchain();

        Application* m_app = nullptr;

        renderer::SDLWindow          m_window;
        renderer::VulkanContext      m_context;
        renderer::VulkanSwapchain    m_swapchain;
        renderer::VulkanRenderPass   m_renderPass;
        renderer::VulkanCommandPool  m_commandPool;
        renderer::VulkanSync         m_sync;
        renderer::VulkanMemoryAllocator m_allocator;
        renderer::ImGuiLayer         m_imguiLayer;

        VkSurfaceKHR m_surface           = VK_NULL_HANDLE;
        u32          m_currentFrameIndex  = 0;
        bool         m_resized            = false;
        u32          m_width              = 1280;
        u32          m_height             = 720;

        SDLInputMapper               m_inputMapper;

        // M7 Global systems
        renderer::Camera               m_camera;
        renderer::VulkanDescriptorPool m_descriptorPool;
        renderer::VulkanBuffer         m_globalUBO;
        VkDescriptorSetLayout          m_globalUBOLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout          m_materialLayout = VK_NULL_HANDLE;
        VkDescriptorSet                m_globalUBOSet = VK_NULL_HANDLE;

        renderer::SpriteRenderPass     m_spritePass;
        renderer::MeshRenderPass       m_meshPass;
        
        // Test assets
        renderer::GpuTexture           m_testTexture;
        renderer::VulkanMaterial       m_testSpriteMaterial;
    };
}
