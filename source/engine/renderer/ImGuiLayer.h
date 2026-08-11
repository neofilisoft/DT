#pragma once

#include "core/platform/Types.h"
#include "runtime/SimulationSnapshot.h"

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

// ---------------------------------------------------------------------------
// ImGuiLayer.h
//
// Wraps Dear ImGui context setup, rendering, and the HUD debug panel.
//
// In DTEngine, ImGui is used for editor panels (Phase 5) and debug overlays.
// The layer is initialized with a Vulkan RenderPass and handles:
//   - ImGui context creation.
//   - ImGui SDL3 platform and Vulkan rendering backend setup.
//   - Font atlas uploading.
//   - Main HUD overlay drawing (showing tick rate, simulation calendar time,
//     needs debugger, speed controls).
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext;
    class VulkanSwapchain;

    class ImGuiLayer
    {
    public:
        ImGuiLayer() = default;
        ~ImGuiLayer();

        ImGuiLayer(const ImGuiLayer&)            = delete;
        ImGuiLayer& operator=(const ImGuiLayer&) = delete;
        ImGuiLayer(ImGuiLayer&&)                 = delete;
        ImGuiLayer& operator=(ImGuiLayer&&)      = delete;

        bool Initialize(VulkanContext&   ctx,
                        VulkanSwapchain& swapchain,
                        VkRenderPass     renderPass,
                        SDL_Window*      window);

        void Shutdown(VulkanContext& ctx);

        // Starts a new ImGui frame (updates inputs, gets dt).
        void BeginFrame();

        // Renders the debug HUD windows on top of the screen.
        // Also draws a needs inspector for a selected agent (or the first agent).
        void DrawHUD(const SimSnapshot& snapshot, f32 measuredTps);

        // Submits the recorded ImGui draw data into the active Vulkan command buffer.
        void Render(VkCommandBuffer cmd);

        bool IsInitialized() const { return m_descriptorPool != VK_NULL_HANDLE; }

    private:
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    };
}
