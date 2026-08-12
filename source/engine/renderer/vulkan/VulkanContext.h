#pragma once

#include "core/platform/Types.h"

#include <vulkan/vulkan.h>

#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// VulkanContext.h
//
// Owns the Vulkan instance, physical device selection, logical device, and
// graphics/present queue handles. This is the first object created during
// renderer initialization and the last destroyed during shutdown.
//
// Ownership: VulkanRenderer owns exactly one VulkanContext by value.
//   All other Vulkan objects are created from VulkanContext::Device() and
//   are destroyed before VulkanContext itself.
//
// Validation layers: enabled in Debug and RelWithDebInfo builds (where
//   DT_SHIPPING is not defined). The debug messenger callback routes all
//   Vulkan validation messages through DT_LOG_* so they appear in the
//   engine's standard log output rather than a separate Vulkan log.
//
// Physical device selection: picks the first discrete GPU, falling back to
//   any GPU that supports the required queue families and extensions.
//   Currently, no scoring of multiple discrete GPUs is implemented - the
//   team's development machines each have one GPU.
//
// Threading: all VulkanContext methods must be called from the render thread.
//   VkDevice and VkQueue handles are not thread-safe for concurrent access
//   without explicit synchronization (which VulkanSync.h provides).
// ---------------------------------------------------------------------------

namespace dt::renderer
{
    class VulkanContext
    {
    public:
        // extensions: instance extensions required by the SDL3 surface
        //   (provided by SDLWindow::GetRequiredInstanceExtensions()).
        // deviceExtensions: device extensions required by the swapchain
        //   (typically just VK_KHR_SWAPCHAIN_EXTENSION_NAME).
        struct CreateInfo
        {
            std::vector<const char*> instanceExtensions;
            std::vector<const char*> deviceExtensions;
            VkSurfaceKHR             surface = VK_NULL_HANDLE; // used for queue family selection
            bool                     enableValidation = false;
        };

        VulkanContext() = default;
        ~VulkanContext();

        // Non-copyable, non-movable: VkDevice/VkInstance are opaque handles
        // that other objects hold references to; moving would invalidate those
        // references without any mechanism to update them.
        VulkanContext(const VulkanContext&)            = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&)                 = delete;
        VulkanContext& operator=(VulkanContext&&)      = delete;

        // Returns false and logs a detailed error if initialization fails.
        // All other methods are undefined to call if Initialize() returned false.
        bool Initialize(const CreateInfo& ci);
        void Shutdown();

        // --- Accessors (render-thread only) --------------------------------

        VkInstance       Instance()       const { return m_instance; }
        VkPhysicalDevice PhysicalDevice() const { return m_physicalDevice; }
        VkDevice         Device()         const { return m_device; }

        // Graphics and present may be the same queue index on unified-memory
        // GPUs; callers must handle the case where they are the same family.
        VkQueue          GraphicsQueue()  const { return m_graphicsQueue; }
        VkQueue          PresentQueue()   const { return m_presentQueue; }
        u32              GraphicsQueueFamilyIndex() const { return m_graphicsQueueFamily; }
        u32              PresentQueueFamilyIndex()  const { return m_presentQueueFamily; }

        // Returns the memory type index that satisfies all bits in
        // `typeFilter` and has all `properties` flags set.
        // Asserts if no matching type is found (programming error: caller
        // must request a type that the physical device actually provides).
        u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const;

        // Helpers for one-shot command recording (used during resource
        // creation, e.g. buffer uploads). Allocates from the transfer
        // command pool, records, submits, and waits for completion.
        VkCommandBuffer BeginOneTimeCommands();
        void            EndOneTimeCommands(VkCommandBuffer cmd);

        bool IsInitialized() const { return m_device != VK_NULL_HANDLE; }

    private:
        bool CreateInstance(const CreateInfo& ci);
        bool SelectPhysicalDevice(VkSurfaceKHR surface, const std::vector<const char*>& deviceExtensions);
        bool CreateLogicalDevice(const std::vector<const char*>& deviceExtensions);
        bool SetupDebugMessenger();

        static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
            VkDebugUtilsMessageTypeFlagsEXT             type,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*                                       userData);

        VkInstance                 m_instance         = VK_NULL_HANDLE;
        VkPhysicalDevice           m_physicalDevice    = VK_NULL_HANDLE;
        VkDevice                   m_device            = VK_NULL_HANDLE;
        VkQueue                    m_graphicsQueue     = VK_NULL_HANDLE;
        VkQueue                    m_presentQueue      = VK_NULL_HANDLE;
        u32                        m_graphicsQueueFamily = 0;
        u32                        m_presentQueueFamily  = 0;
        VkDebugUtilsMessengerEXT   m_debugMessenger    = VK_NULL_HANDLE;
        VkCommandPool              m_oneTimePool       = VK_NULL_HANDLE;

        bool m_validationEnabled = false;
    };
}
