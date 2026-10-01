// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/VulkanRenderer.h"

#include "core/filesystem/FileSystem.h"
#include "core/input/InputManager.h"
#include "core/logging/Logger.h"
#include "runtime/Application.h"

#include <SDL3/SDL_vulkan.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

#include <vector>

namespace lacrima
{
    static constexpr u32 kMaxFramesInFlight = 2;

    VulkanRenderer::VulkanRenderer()
    {
    }

    bool VulkanRenderer::Initialize()
    {
        LACRIMA_LOG_INFO(LogCategory::Renderer, "VulkanRenderer: initializing window and Vulkan backend...");

        // 1. Initialize SDL Window
        if (!m_window.Initialize(m_windowTitle, m_width, m_height))
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: window initialization failed");
            return false;
        }

        // 2. Gather extensions & initialize Vulkan Context
        std::vector<const char*> instanceExtensions = m_window.GetRequiredInstanceExtensions();
        std::vector<const char*> deviceExtensions   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

        renderer::VulkanContext::CreateInfo ctxCI{};
        ctxCI.instanceExtensions = instanceExtensions;
        ctxCI.deviceExtensions   = deviceExtensions;
        ctxCI.enableValidation   = true; 

        // 3. Initialize Vulkan Instance
        if (!m_context.InitializeInstance(ctxCI))
        {
            return false;
        }

        // 4. Create Window Surface
        m_surface = m_window.CreateSurface(m_context.Instance());
        if (m_surface == VK_NULL_HANDLE)
            return false;

        // 5. Initialize Logical Device & Select Physical Device
        if (!m_context.InitializeDevice(m_surface, deviceExtensions))
        {
            SDL_Vulkan_DestroySurface(m_context.Instance(), m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
            return false;
        }

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

        const VkFormat depthFormat = m_context.FindDepthFormat();
        if (!m_renderPass.Initialize(m_context, colorFormat, depthFormat)) return false;
        if (!m_swapchain.Initialize(m_context, m_surface, m_renderPass.Handle(), m_width, m_height)) return false;
        if (!m_commandPool.Initialize(m_context, m_swapchain.ImageCount())) return false;
        if (!m_sync.Initialize(m_context, m_swapchain.ImageCount())) return false;
        if (!m_allocator.Initialize(&m_context)) return false;
        
        // Systems Initialization
        m_camera.SetPerspective(math::DegToRad(60.0f), (float)m_width / (float)m_height, 0.1f, 1000.0f);
        m_camera.LookAt(Vec3(0.0f, 4.0f, 7.0f), Vec3(0.0f, 0.9f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

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

        // Default PBR Textures
        u32 whitePixel = 0xFFFFFFFF; // RGBA White (Albedo, AO)
        u32 flatNormal = 0xFFFF8080; // RGBA (128, 128, 255, 255) -> ABGR little endian
        u32 blackPixel = 0xFF000000; // RGBA Black (Emissive)
        u32 defaultMR  = 0xFFFF0000; // RGBA (0, 0, 255, 255) -> metallic=0, roughness=1 (roughness is G channel, wait: freely says bg = metallic, roughness)
        // freely: vec2 mr = texture(...).bg; metallic = mr.x (B), roughness = mr.y (G)
        // so we want B=0, G=255. ABGR: A=255, B=0, G=255, R=0 -> 0xFF00FF00
        u32 defaultMRPixel = 0xFF00FF00; 

        m_defaultAlbedo.LoadFromMemory(m_context, m_allocator, &whitePixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);
        m_defaultNormal.LoadFromMemory(m_context, m_allocator, &flatNormal, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);
        m_defaultMR.LoadFromMemory(m_context, m_allocator, &defaultMRPixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);
        m_defaultAO.LoadFromMemory(m_context, m_allocator, &whitePixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);
        m_defaultEmissive.LoadFromMemory(m_context, m_allocator, &blackPixel, 1, 1, VK_FORMAT_R8G8B8A8_UNORM);

        if (!m_projectTexturePath.empty())
        {
            if (!m_testTexture.LoadFromCookedFile(m_context, m_allocator,
                                                  m_projectTexturePath,
                                                  m_projectTexturePixelPerfect))
                return false;
            if (!m_testSpriteMaterial.Initialize(m_context, m_descriptorPool, m_materialLayout, m_testTexture, m_defaultNormal, m_defaultMR, m_defaultAO, m_defaultEmissive))
                return false;
            m_projectMaterial = &m_testSpriteMaterial;
        }
        // The engine renderer does not load assets from any sample game.
        // A project owns its texture/material/mesh bindings and may inject them
        // through its renderer integration layer.

        // Initialize ImGuiLayer first so that ImGui Vulkan backend is fully initialized
        if (!m_imguiLayer.Initialize(m_context, m_swapchain, m_renderPass.Handle(), m_window.Handle()))
            return false;

        // Initialize Offscreen Viewport Target (safely registers ImGui descriptor now)
        if (!m_offscreenTarget.Initialize(m_context, m_width, m_height))
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: offscreen target init failed");
            return false;
        }

        // Pass Init using Offscreen RenderPass
        m_spritePass.Initialize(m_context, m_offscreenTarget.Handle(), m_globalUBOLayout, m_materialLayout);
        m_meshPass.Initialize(m_context, m_offscreenTarget.Handle(), m_globalUBOLayout, m_materialLayout);
        m_raycastPass.Initialize(m_context, m_offscreenTarget.Handle(), m_globalUBOLayout, m_materialLayout);
        m_skinnedMeshPass.Initialize(m_context, m_offscreenTarget.Handle(), m_globalUBOLayout, m_materialLayout);
        m_csmPass.Initialize(m_context);

        // Load input bindings from config. Fallback to engine defaults on failure.
        InputManager::Get().LoadBindings(FileSystem::GetEngineAssetDir() + "/input.ini");
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
            m_raycastPass.Shutdown(m_context);
            m_skinnedMeshPass.Shutdown(m_context);
            m_csmPass.Shutdown(m_context);
            m_playerMesh.Shutdown(m_context, m_allocator);
            m_testTexture.Shutdown(m_context, m_allocator);

            m_offscreenTarget.Shutdown(m_context);
            m_imguiLayer.Shutdown(m_context);
            InputManager::Get().CloseGamepads();
            m_sync.Shutdown(m_context);
            m_commandPool.Shutdown(m_context);
            m_swapchain.Shutdown(m_context);
            m_renderPass.Shutdown(m_context);

            vkDestroyDescriptorSetLayout(m_context.Device(), m_materialLayout, nullptr);
            vkDestroyDescriptorSetLayout(m_context.Device(), m_globalUBOLayout, nullptr);
            m_globalUBO.Shutdown(m_context);
        m_defaultAlbedo.Shutdown(m_context, m_allocator);
        m_defaultNormal.Shutdown(m_context, m_allocator);
        m_defaultMR.Shutdown(m_context, m_allocator);
        m_defaultAO.Shutdown(m_context, m_allocator);
        m_defaultEmissive.Shutdown(m_context, m_allocator);
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

    bool VulkanRenderer::Render(const SimSnapshot& snapshot)
    {
        if (m_pendingOffscreenResize)
        {
            m_pendingOffscreenResize = false;
            m_offscreenTarget.Resize(m_context, m_pendingOffscreenWidth, m_pendingOffscreenHeight);
        }

        VkDevice device = m_context.Device();

        // 1. Process OS window/keyboard events on the render thread
        InputManager::Get().BeginFrame();
        bool resized = false;
        u32 newWidth = 0;
        u32 newHeight = 0;
        if (!m_window.PollEvents(resized, newWidth, newHeight)) { return false; }

        InputManager::Get().EndFrame();

        if (resized || m_resized)
        {
            m_resized = false;
            m_width   = newWidth;
            m_height  = newHeight;
            RecreateSwapchain();
            return true;
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
            return true;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: failed to acquire swapchain image");
            return true;
        }

        // Reset the fence only if we are successfully submitting work
        vkResetFences(device, 1, &inFlightFence);

        // 4. Record command buffer
        VkCommandBuffer cmd = m_commandPool.Buffer(imageIndex);
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        renderer::GlobalUniforms globals{};
        if (m_useCustomCamera)
        {
            globals.viewMatrix = m_customView;
            globals.projectionMatrix = m_customProj;
            globals.viewProjectionMatrix = globals.projectionMatrix * globals.viewMatrix;
            globals.cameraPosition = Vec4(m_customCameraPos, 1.0f);
        }
        else
        {
            m_camera.Update();
            globals.viewMatrix = m_camera.GetViewMatrix();
            globals.projectionMatrix = m_camera.GetProjectionMatrix();
            globals.viewProjectionMatrix = globals.projectionMatrix * globals.viewMatrix;
            globals.cameraPosition = Vec4(m_camera.GetPosition(), 1.0f);
        }
        globals.lightDirection = Vec4(-0.35f, -1.0f, -0.25f, 0.0f);
        globals.lightColor = Vec4(1.0f, 0.92f, 0.78f, 1.35f);
        globals.ambientColor = Vec4(0.16f, 0.18f, 0.24f, 1.0f);
        m_globalUBO.CopyData(m_context, &globals, sizeof(globals));

        m_meshProxies.clear();
        for (const auto& proxy : snapshot.proxies)
        {
            if (proxy.visualId == StringID("hero"))
                m_meshProxies.push_back(proxy);
        }

        // --- Pass 1: 3D Scene rendered to Offscreen Target ---
        if (m_offscreenTarget.IsInitialized())
        {
            VkRenderPassBeginInfo offscreenPassInfo{};
            offscreenPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            offscreenPassInfo.renderPass        = m_offscreenTarget.Handle();
            offscreenPassInfo.framebuffer       = m_offscreenTarget.Framebuffer();
            offscreenPassInfo.renderArea.offset = { 0, 0 };
            offscreenPassInfo.renderArea.extent = m_offscreenTarget.Extent();

            VkClearValue offscreenClearValues[2]{};
            offscreenClearValues[0].color = { { 0.10f, 0.11f, 0.14f, 1.0f } };
            offscreenClearValues[1].depthStencil = { 1.0f, 0 };
            offscreenPassInfo.clearValueCount = 2;
            offscreenPassInfo.pClearValues    = offscreenClearValues;

            vkCmdBeginRenderPass(cmd, &offscreenPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            m_spritePass.SetupFrame(m_offscreenTarget.Extent(), m_globalUBOSet,
                                    (m_projectMaterial ? m_projectMaterial : &m_testSpriteMaterial),
                                    &snapshot.proxies);
            m_spritePass.Execute(cmd);

            if (!snapshot.raycastColumns.empty())
            {
                m_raycastPass.SetupFrame(m_offscreenTarget.Extent(), m_globalUBOSet,
                                         (m_projectMaterial ? m_projectMaterial : &m_testSpriteMaterial),
                                         &snapshot.raycastColumns);
                m_raycastPass.Execute(cmd);
            }

            m_meshPass.SetupFrame(m_offscreenTarget.Extent(), m_globalUBOSet,
                                  &m_testSpriteMaterial, &m_playerMesh, &m_meshProxies);
            m_meshPass.Execute(cmd);

            vkCmdEndRenderPass(cmd);
        }

        // --- Pass 2: ImGui UI & Viewport Image rendered to Swapchain ---
        VkRenderPassBeginInfo swapchainPassInfo{};
        swapchainPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        swapchainPassInfo.renderPass        = m_renderPass.Handle();
        swapchainPassInfo.framebuffer       = m_swapchain.Framebuffer(imageIndex);
        swapchainPassInfo.renderArea.offset = { 0, 0 };
        swapchainPassInfo.renderArea.extent = m_swapchain.Extent();

        VkClearValue clearValues[2]{};
        clearValues[0].color = { { 0.08f, 0.09f, 0.12f, 1.0f } };
        clearValues[1].depthStencil = { 1.0f, 0 };
        swapchainPassInfo.clearValueCount = 2;
        swapchainPassInfo.pClearValues    = clearValues;

        vkCmdBeginRenderPass(cmd, &swapchainPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        if (m_imguiLayer.IsInitialized())
        {
            m_imguiLayer.BeginFrame();
            if (m_editorConstructCallback)
            {
                m_editorConstructCallback();
            }

            ImGui::Render();
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

        VkSemaphore signalSemaphores[] = { m_sync.RenderFinishedSemaphore(imageIndex) };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = signalSemaphores;

        if (vkQueueSubmit(m_context.GraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS)
        {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: failed to submit draw commands to queue");
            return true;
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
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: swapchain present failed");
        }

                m_currentFrameIndex = (m_currentFrameIndex + 1) % kMaxFramesInFlight;

        if (m_imguiLayer.IsInitialized())
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                ImGui::UpdatePlatformWindows();
                ImGui::RenderPlatformWindowsDefault();
            }
        }
        return true;
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
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "VulkanRenderer: failed to recreate swapchain");
            return;
        }

        if (m_imguiLayer.IsInitialized())
        {
            ImGui_ImplVulkan_SetMinImageCount(m_swapchain.ImageCount());
        }
    }
}















