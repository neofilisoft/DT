#include "renderer/VulkanRenderer.h"

#include "core/input/InputManager.h"
#include "core/logging/Logger.h"
#include "runtime/Application.h"

#include <SDL3/SDL_vulkan.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <vector>

namespace dt
{
    static constexpr u32 kMaxFramesInFlight = 2;

    VulkanRenderer::VulkanRenderer()
    {
    }

    bool VulkanRenderer::Initialize()
    {
        DT_LOG_INFO(LogCategory::Renderer, "VulkanRenderer: initializing window and Vulkan backend...");

        // 1. Initialize SDL Window
        if (!m_window.Initialize("Domaintic - DTEngine", m_width, m_height))
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: window initialization failed");
            return false;
        }

        // 2. Gather extensions & initialize Vulkan Context
        std::vector<const char*> instanceExtensions = m_window.GetRequiredInstanceExtensions();
        std::vector<const char*> deviceExtensions   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        renderer::VulkanContext::CreateInfo ctxCI{};
        ctxCI.instanceExtensions = instanceExtensions;
        ctxCI.deviceExtensions   = deviceExtensions;
        ctxCI.enableValidation   = true; 

        // Temporary Vulkan instance to create surface for physical device selection
        VkInstance tempInstance = VK_NULL_HANDLE;
        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "DomainticTemp";
        appInfo.apiVersion         = VK_API_VERSION_1_2;

        VkInstanceCreateInfo instCI{};
        instCI.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instCI.pApplicationInfo        = &appInfo;
        instCI.enabledExtensionCount   = static_cast<u32>(instanceExtensions.size());
        instCI.ppEnabledExtensionNames = instanceExtensions.data();

        if (vkCreateInstance(&instCI, nullptr, &tempInstance) != VK_SUCCESS)
            return false;

        VkSurfaceKHR tempSurface = m_window.CreateSurface(tempInstance);
        if (tempSurface == VK_NULL_HANDLE)
        {
            vkDestroyInstance(tempInstance, nullptr);
            return false;
        }

        ctxCI.surface = tempSurface;

        if (!m_context.Initialize(ctxCI))
        {
            SDL_Vulkan_DestroySurface(tempInstance, tempSurface, nullptr);
            vkDestroyInstance(tempInstance, nullptr);
            return false;
        }

        SDL_Vulkan_DestroySurface(tempInstance, tempSurface, nullptr);
        vkDestroyInstance(tempInstance, nullptr);

        m_surface = m_window.CreateSurface(m_context.Instance());
        if (m_surface == VK_NULL_HANDLE)
            return false;

        VkSurfaceFormatKHR surfaceFormat{};
        u32 formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.PhysicalDevice(), m_surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_context.PhysicalDevice(), m_surface, &formatCount, formats.data());
        
        VkFormat colorFormat = VK_FORMAT_B8G8R8A8_SRGB;
        if (!formats.empty())
        {
            bool found = false;
            for (const auto& fmt : formats)
            {
                if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB)
                {
                    colorFormat = fmt.format;
                    found = true;
                    break;
                }
            }
            if (!found) colorFormat = formats[0].format;
        }

        if (!m_renderPass.Initialize(m_context, colorFormat)) return false;
        if (!m_swapchain.Initialize(m_context, m_surface, m_renderPass.Handle(), m_width, m_height)) return false;
        if (!m_commandPool.Initialize(m_context, m_swapchain.ImageCount())) return false;
        if (!m_sync.Initialize(m_context, kMaxFramesInFlight)) return false;
        if (!m_allocator.Initialize(&m_context)) return false;
        
        // Systems Initialization
        m_camera.SetPerspective(math::DegToRad(60.0f), (float)m_width / (float)m_height, 0.1f, 1000.0f);
        m_camera.SetTransform(Vec3(0.0f, 10.0f, 10.0f), math::DegToRad(-45.0f), 0.0f);

        if (!m_descriptorPool.Initialize(m_context, 10, 10, 10)) return false;

        // Global UBO
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        
        VkDescriptorSetLayoutCreateInfo uboLayoutInfo{};
        uboLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        uboLayoutInfo.bindingCount = 1;
        uboLayoutInfo.pBindings = &uboLayoutBinding;
        vkCreateDescriptorSetLayout(m_context.Device(), &uboLayoutInfo, nullptr, &m_globalUBOLayout);

        if (!m_globalUBO.Initialize(m_context, sizeof(renderer::GlobalUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
            return false;

        m_descriptorPool.AllocateDescriptorSet(m_context, m_globalUBOLayout, m_globalUBOSet);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_globalUBO.Handle();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(renderer::GlobalUniforms);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = m_globalUBOSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(m_context.Device(), 1, &descriptorWrite, 0, nullptr);

        // Material Layout
        renderer::VulkanMaterial::CreateDescriptorSetLayout(m_context, m_materialLayout);

        // Load test texture and create material
        if (m_testTexture.LoadFromCookedFile(m_context, m_allocator, "source/engine/asset/PixelSpaces_Free_Pack.asset"))
        {
            m_testSpriteMaterial.Initialize(m_context, m_descriptorPool, m_materialLayout, m_testTexture);
        }

        // Pass Init
        m_spritePass.Initialize(m_context, m_renderPass.Handle(), m_globalUBOLayout, m_materialLayout);
        m_meshPass.Initialize(m_context, m_renderPass.Handle(), m_globalUBOLayout, m_materialLayout);

        if (!m_imguiLayer.Initialize(m_context, m_swapchain, m_renderPass.Handle(), m_window.Handle()))
            return false;

        // GameUILayer shares the ImGui context - must init AFTER ImGuiLayer.
        m_gameUILayer.Initialize();

        // Load input bindings from config. Fallback to engine defaults on failure.
        InputManager::Get().LoadBindings("source/engine/asset/input.ini");
        InputManager::Get().OpenGamepads();

        return true;
    }

    void VulkanRenderer::Shutdown()
    {
        if (m_context.Device() != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_context.Device());

            m_spritePass.Shutdown(m_context);
            m_meshPass.Shutdown(m_context);
            m_testTexture.Shutdown(m_context, m_allocator);

            m_imguiLayer.Shutdown(m_context);
            m_gameUILayer.Shutdown();
            InputManager::Get().CloseGamepads();
            m_sync.Shutdown(m_context);
            m_commandPool.Shutdown(m_context);
            m_swapchain.Shutdown(m_context);
            m_renderPass.Shutdown(m_context);

            vkDestroyDescriptorSetLayout(m_context.Device(), m_materialLayout, nullptr);
            vkDestroyDescriptorSetLayout(m_context.Device(), m_globalUBOLayout, nullptr);
            m_globalUBO.Shutdown(m_context);
            m_descriptorPool.Shutdown(m_context);
            m_allocator.Shutdown(&m_context);

            if (m_surface != VK_NULL_HANDLE)
            {
                SDL_Vulkan_DestroySurface(m_context.Instance(), m_surface, nullptr);
                m_surface = VK_NULL_HANDLE;
            }

            m_context.Shutdown();
        }

        m_window.Shutdown();
    }

    void VulkanRenderer::Render(const SimSnapshot& snapshot)
    {
        VkDevice device = m_context.Device();

        // 1. Process OS window/keyboard events on the render thread
        bool resized = false;
        u32 newWidth = 0;
        u32 newHeight = 0;
        if (!m_window.PollEvents(resized, newWidth, newHeight))
        {
            if (m_app != nullptr)
            {
                m_app->RequestShutdown();
            }
            return;
        }

        // Process all pending SDL events - keyboard, gamepad, window, touch.
        // InputManager::BeginFrame clears "just pressed" flags before we feed events.
        InputManager::Get().BeginFrame();
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            // Let ImGui see every event first (for text fields, mouse in debug UI).
            ImGui_ImplSDL3_ProcessEvent(&ev);

            if (ev.type == SDL_EVENT_QUIT)
            {
                if (m_app != nullptr)
                    m_app->RequestShutdown();
                return;
            }

            if (m_app != nullptr)
            {
                if (!m_inputMapper.ProcessEvent(ev, *m_app))
                    return;
            }
        }
        InputManager::Get().EndFrame();

        if (resized || m_resized)
        {
            m_resized = false;
            m_width   = newWidth;
            m_height  = newHeight;
            RecreateSwapchain();
            return;
        }

        // 2. CPU-GPU Frame synchronization
        VkFence inFlightFence = m_sync.InFlightFence(m_currentFrameIndex);
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

        // 3. Acquire swapchain image
        VkSemaphore imageAvailableSem = m_sync.ImageAvailableSemaphore(m_currentFrameIndex);
        u32 imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(device, m_swapchain.Handle(), UINT64_MAX,
                                                imageAvailableSem, VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapchain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: failed to acquire swapchain image");
            return;
        }

        // Reset the fence only if we are successfully submitting work
        vkResetFences(device, 1, &inFlightFence);

        // 4. Record command buffer
        VkCommandBuffer cmd = m_commandPool.Buffer(imageIndex);
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        // Begin render pass with dark premium background color (0.08, 0.09, 0.12)
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = m_renderPass.Handle();
        renderPassInfo.framebuffer       = m_swapchain.Framebuffer(imageIndex);
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapchain.Extent();

        VkClearValue clearColor = { { { 0.08f, 0.09f, 0.12f, 1.0f } } };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues    = &clearColor;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Draw agents
        m_spritePass.SetupFrame(m_swapchain.Extent(), m_globalUBOSet, &m_testSpriteMaterial, &snapshot.proxies);
        m_spritePass.Execute(cmd);

        // Draw HUD overlay - Game HUD first, then engine Debug HUD on top
        if (m_imguiLayer.IsInitialized())
        {
            m_imguiLayer.BeginFrame();

            // Game HUD (day/time, needs bars, entity selector)
            m_gameUILayer.DrawGameHUD(snapshot, m_width, m_height);

            // Engine Debug HUD (simulation inspector, profiler, speed controls)
            float tps = 0.0f;
            if (m_app != nullptr)
                tps = m_app->Sim().MeasuredTicksPerSecond();

            m_imguiLayer.DrawHUD(snapshot, tps);
            m_imguiLayer.Render(cmd);
        }

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        // 5. Submit command buffer to Graphics Queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[]      = { imageAvailableSem };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount     = 1;
        submitInfo.pWaitSemaphores        = waitSemaphores;
        submitInfo.pWaitDstStageMask      = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;

        VkSemaphore signalSemaphores[] = { m_sync.RenderFinishedSemaphore(m_currentFrameIndex) };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = signalSemaphores;

        if (vkQueueSubmit(m_context.GraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: failed to submit draw commands to queue");
            return;
        }

        // 6. Present render results to screen
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = signalSemaphores;

        VkSwapchainKHR swapchains[] = { m_swapchain.Handle() };
        presentInfo.swapchainCount  = 1;
        presentInfo.pSwapchains     = swapchains;
        presentInfo.pImageIndices   = &imageIndex;

        result = vkQueuePresentKHR(m_context.PresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            RecreateSwapchain();
        }
        else if (result != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: swapchain present failed");
        }

        m_currentFrameIndex = (m_currentFrameIndex + 1) % kMaxFramesInFlight;
    }

    void VulkanRenderer::RecreateSwapchain()
    {
        VkDevice device = m_context.Device();
        vkDeviceWaitIdle(device);

        // Handle minimized windows by sleeping until dimensions are non-zero
        int width = 0, height = 0;
        SDL_GetWindowSize(m_window.Handle(), &width, &height);
        while (width == 0 || height == 0)
        {
            SDL_GetWindowSize(m_window.Handle(), &width, &height);
            SDL_Delay(10);
            SDL_Event ev;
            SDL_PollEvent(&ev);
        }

        m_width  = static_cast<u32>(width);
        m_height = static_cast<u32>(height);

        if (!m_swapchain.Recreate(m_context, m_surface, m_renderPass.Handle(), m_width, m_height))
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: failed to recreate swapchain");
            return;
        }

        if (m_imguiLayer.IsInitialized())
        {
            ImGui_ImplVulkan_SetMinImageCount(m_swapchain.ImageCount());
        }
    }
}
