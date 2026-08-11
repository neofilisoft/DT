#include "renderer/ImGuiLayer.h"

#include "core/logging/Logger.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanSwapchain.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

namespace dt::renderer
{
    ImGuiLayer::~ImGuiLayer()
    {
        DT_ASSERT(m_descriptorPool == VK_NULL_HANDLE,
            "ImGuiLayer destroyed without calling Shutdown() - resource leak");
    }

    bool ImGuiLayer::Initialize(VulkanContext&   ctx,
                                VulkanSwapchain& swapchain,
                                VkRenderPass     renderPass,
                                SDL_Window*      window)
    {
        VkDevice device = ctx.Device();

        // 1. Create Descriptor Pool for ImGui
        VkDescriptorPoolSize poolSizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 10 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10 }
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 110; // 10 * 11
        poolInfo.poolSizeCount = static_cast<u32>(std::size(poolSizes));
        poolInfo.pPoolSizes    = poolSizes;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "ImGuiLayer: failed to create ImGui VkDescriptorPool");
            return false;
        }

        // 2. Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Disabled for non-docking ImGui branch

        // Set dark styling matching Neofilisoft/Balmung premium theme requirements
        ImGui::StyleColorsDark();

        // Adjust padding and rounding for a premium modern aesthetic
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding    = 8.0f;
        style.FrameRounding     = 6.0f;
        style.GrabRounding      = 4.0f;
        style.PopupRounding     = 6.0f;
        style.ScrollbarRounding = 12.0f;
        style.WindowBorderSize  = 1.0f;
        style.FrameBorderSize   = 1.0f;

        // 3. Initialize Platform/Renderer backends
        if (!ImGui_ImplSDL3_InitForVulkan(window))
        {
            DT_LOG_ERROR(LogCategory::Renderer, "ImGuiLayer: failed to initialize ImGui SDL3 platform backend");
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
            ImGui::DestroyContext();
            return false;
        }

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion          = VK_API_VERSION_1_2;
        initInfo.Instance            = ctx.Instance();
        initInfo.PhysicalDevice      = ctx.PhysicalDevice();
        initInfo.Device              = device;
        initInfo.QueueFamily         = ctx.GraphicsQueueFamilyIndex();
        initInfo.Queue               = ctx.GraphicsQueue();
        initInfo.DescriptorPool      = m_descriptorPool;
        initInfo.MinImageCount       = swapchain.ImageCount(); // >= 2
        initInfo.ImageCount          = swapchain.ImageCount();
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.PipelineInfoMain.RenderPass = renderPass;

        if (!ImGui_ImplVulkan_Init(&initInfo))
        {
            DT_LOG_ERROR(LogCategory::Renderer, "ImGuiLayer: failed to initialize ImGui Vulkan rendering backend");
            ImGui_ImplSDL3_Shutdown();
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
            ImGui::DestroyContext();
            return false;
        }

        // Font texture is uploaded automatically inside NewFrame in this ImGui version.

        DT_LOG_INFO(LogCategory::Renderer, "ImGuiLayer: initialized successfully");
        return true;
    }

    void ImGuiLayer::Shutdown(VulkanContext& ctx)
    {
        if (m_descriptorPool == VK_NULL_HANDLE)
            return;

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();

        vkDestroyDescriptorPool(ctx.Device(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;

        DT_LOG_INFO(LogCategory::Renderer, "ImGuiLayer: shut down");
    }

    void ImGuiLayer::BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::DrawHUD(const SimSnapshot& snapshot, f32 measuredTps)
    {
        // -----------------------------------------------------------------------
        // Clock / Calendar Display
        // Matches the calendar calculation defined in SimClock.h:
        //   totalGameMinutes = tickIndex / 25
        // -----------------------------------------------------------------------
        const u64 totalGameMinutes = snapshot.tickIndex / 25;
        const u64 minute           = totalGameMinutes % 60;
        const u64 hour             = (totalGameMinutes / 60) % 24;
        const u64 day              = totalGameMinutes / (60 * 24);
        const u64 dayOfWeek        = day % 7;

        static constexpr const char* kDayNames[] = {
            "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
        };

        // 1. Simulation Info Window (Compact Top-Left overlay)
        ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 130.0f), ImGuiCond_Always);
        ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration
                                      | ImGuiWindowFlags_AlwaysAutoResize
                                      | ImGuiWindowFlags_NoSavedSettings
                                      | ImGuiWindowFlags_NoFocusOnAppearing
                                      | ImGuiWindowFlags_NoNav;

        ImGui::Begin("Simulation HUD Overlay", nullptr, overlayFlags);
        {
            ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.03f, 1.0f), "DTEngine M5 Runtime Monitor");
            ImGui::Separator();

            ImGui::Text("Tick Index:  %llu", snapshot.tickIndex);
            ImGui::Text("TPS (Sim):   %.1f ticks/sec", measuredTps);
            ImGui::Text("Speed Scale: %.1fx %s", snapshot.timeScale, (snapshot.timeScale == 0.0f) ? "(PAUSED)" : "");

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.13f, 0.59f, 0.95f, 1.0f),
                "Calendar:    Day %llu (%s) - %02llu:%02llu",
                day + 1, kDayNames[dayOfWeek], hour, minute);
        }
        ImGui::End();

        // 2. Control Guides Overlay (Compact Top-Right overlay)
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 260.0f, 10.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(250.0f, 110.0f), ImGuiCond_Always);
        ImGui::Begin("Keyboard Shortcuts Guide", nullptr, overlayFlags);
        {
            ImGui::TextColored(ImVec4(0.30f, 0.69f, 0.31f, 1.0f), "Keyboard Controls");
            ImGui::Separator();
            ImGui::Text("[Space] : Pause / Resume");
            ImGui::Text("[1]     : Normal Speed (1x)");
            ImGui::Text("[2]     : Fast Speed (2x)");
            ImGui::Text("[8]     : Ultra Speed (8x)");
        }
        ImGui::End();

        // 3. Simulated Entities Inspector
        ImGui::SetNextWindowPos(ImVec2(10.0f, 150.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350.0f, 400.0f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Simulation Inspector");
        {
            ImGui::Text("Live Agents in World: %zu", snapshot.proxies.size());
            ImGui::Separator();

            if (snapshot.proxies.empty())
            {
                ImGui::Text("No active entities in the simulation.");
            }
            else
            {
                static int selectedAgent = 0;
                if (selectedAgent >= static_cast<int>(snapshot.proxies.size()))
                    selectedAgent = 0;

                // Agent selector list
                ImGui::Text("Select Agent to Inspect:");
                if (ImGui::BeginCombo("##AgentSelector", ("Agent #" + std::to_string(selectedAgent)).c_str()))
                {
                    for (int i = 0; i < static_cast<int>(snapshot.proxies.size()); ++i)
                    {
                        const bool isSelected = (selectedAgent == i);
                        if (ImGui::Selectable(("Agent #" + std::to_string(i)).c_str(), isSelected))
                        {
                            selectedAgent = i;
                        }
                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Separator();

                // Draw proxy details
                const auto& proxy = snapshot.proxies[selectedAgent];
                ImGui::Text("Entity Handle ID: 0x%016llX", proxy.entityId);
                ImGui::Text("Position XZ:      (%.3f, %.3f)", proxy.positionX, proxy.positionZ);
                ImGui::Text("Archetype ID:     %u", proxy.archetypeId);
                ImGui::Text("Animation State:  %u", proxy.animationState);

                ImGui::Separator();

                // Note on simulation needs:
                // Since M1, the Simulation writes flat proxies into SimSnapshot.
                // It does NOT expose NeedsComponent directly to the renderer to keep
                // thread separation. In the final game (Phase 6), needs data can be
                // added to the snapshot. For M5, we present this architecture note.
                ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "M5 Architectural Note:");
                ImGui::TextWrapped("The renderer does not hold handles to live Simulation state. "
                                   "Needs decay and Autonomy scoring run entirely headless on "
                                   "the Simulation thread. Simulation stats are written out "
                                   "via read-only snapshots to this UI thread.");
            }
        }
        ImGui::End();

        ImGui::Render();
    }

    void ImGuiLayer::Render(VkCommandBuffer cmd)
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }
}
