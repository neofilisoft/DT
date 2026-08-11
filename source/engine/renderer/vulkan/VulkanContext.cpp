#include "renderer/vulkan/VulkanContext.h"

#include "core/logging/Logger.h"
#include "core/platform/Types.h"
#include "core/platform/Assert.h"

#include <vulkan/vulkan.h>

#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace dt::renderer
{
    // Validation layer and extension names used in debug builds.
    static constexpr const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";
    static constexpr const char* kDebugUtilsExtName   = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    // ---------------------------------------------------------------------------

    bool VulkanContext::Initialize(const CreateInfo& ci)
    {
        m_validationEnabled = ci.enableValidation;

        if (!CreateInstance(ci))
            return false;

        if (m_validationEnabled)
        {
            if (!SetupDebugMessenger())
            {
                DT_LOG_WARN(LogCategory::Renderer,
                    "VulkanContext: debug messenger setup failed - validation output will be missing");
            }
        }

        if (!SelectPhysicalDevice(ci.surface, ci.deviceExtensions))
            return false;

        if (!CreateLogicalDevice(ci.deviceExtensions))
            return false;

        // One-time command pool for buffer copies and image layout transitions
        // performed during resource creation (outside the per-frame record loop).
        VkCommandPoolCreateInfo poolCI{};
        poolCI.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolCI.queueFamilyIndex = m_graphicsQueueFamily;
        poolCI.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

        if (vkCreateCommandPool(m_device, &poolCI, nullptr, &m_oneTimePool) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer,
                "VulkanContext: failed to create one-time command pool");
            return false;
        }

        DT_LOG_INFO(LogCategory::Renderer,
            "VulkanContext: initialized (validation={})", m_validationEnabled);
        return true;
    }

    void VulkanContext::Shutdown()
    {
        if (m_device == VK_NULL_HANDLE)
            return;

        if (m_oneTimePool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_device, m_oneTimePool, nullptr);
            m_oneTimePool = VK_NULL_HANDLE;
        }

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;

        if (m_validationEnabled && m_debugMessenger != VK_NULL_HANDLE)
        {
            auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyFn)
                destroyFn(m_instance, m_debugMessenger, nullptr);
            m_debugMessenger = VK_NULL_HANDLE;
        }

        if (m_instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }

        DT_LOG_INFO(LogCategory::Renderer, "VulkanContext: shut down");
    }

    VulkanContext::~VulkanContext()
    {
        Shutdown();
    }

    // ---------------------------------------------------------------------------

    bool VulkanContext::CreateInstance(const CreateInfo& ci)
    {
        // Validate requested validation layer availability
        if (m_validationEnabled)
        {
            u32 layerCount = 0;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> layers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, layers.data());

            bool found = false;
            for (const auto& layer : layers)
            {
                if (std::strcmp(layer.layerName, kValidationLayerName) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                DT_LOG_WARN(LogCategory::Renderer,
                    "VulkanContext: validation layer '{}' not available - "
                    "install the Vulkan SDK or run without validation",
                    kValidationLayerName);
                m_validationEnabled = false;
            }
        }

        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "Domaintic";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 5, 0); // M5
        appInfo.pEngineName        = "DTEngine";
        appInfo.engineVersion      = VK_MAKE_VERSION(0, 5, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_2;

        // Collect extensions: SDL3 required + optional debug utils
        std::vector<const char*> extensions = ci.instanceExtensions;
        if (m_validationEnabled)
            extensions.push_back(kDebugUtilsExtName);

        std::vector<const char*> validationLayers;
        if (m_validationEnabled)
            validationLayers.push_back(kValidationLayerName);

        VkInstanceCreateInfo instCI{};
        instCI.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instCI.pApplicationInfo        = &appInfo;
        instCI.enabledExtensionCount   = static_cast<u32>(extensions.size());
        instCI.ppEnabledExtensionNames = extensions.data();
        instCI.enabledLayerCount       = static_cast<u32>(validationLayers.size());
        instCI.ppEnabledLayerNames     = validationLayers.data();

        if (vkCreateInstance(&instCI, nullptr, &m_instance) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer,
                "VulkanContext: vkCreateInstance failed");
            return false;
        }
        return true;
    }

    bool VulkanContext::SelectPhysicalDevice(VkSurfaceKHR surface,
                                              const std::vector<const char*>& deviceExtensions)
    {
        u32 count = 0;
        vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
        if (count == 0)
        {
            DT_LOG_ERROR(LogCategory::Renderer,
                "VulkanContext: no Vulkan-capable GPU found");
            return false;
        }

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(m_instance, &count, devices.data());

        // Score function: prefer discrete GPU, then any GPU that has required
        // extensions and queue families.
        auto deviceScore = [&](VkPhysicalDevice dev) -> int
        {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(dev, &props);

            // Check required device extensions
            u32 extCount = 0;
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
            std::vector<VkExtensionProperties> exts(extCount);
            vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, exts.data());

            std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
            for (const auto& ext : exts)
                required.erase(ext.extensionName);
            if (!required.empty())
                return -1; // missing a required extension

            // Check graphics queue family
            u32 queueCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, nullptr);
            std::vector<VkQueueFamilyProperties> queues(queueCount);
            vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueCount, queues.data());

            bool hasGraphics = false;
            bool hasPresent  = false;
            for (u32 i = 0; i < queueCount; ++i)
            {
                if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    hasGraphics = true;

                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
                if (presentSupport)
                    hasPresent = true;
            }
            if (!hasGraphics || !hasPresent)
                return -1;

            // Check swapchain support
            u32 formatCount  = 0;
            u32 presentCount = 0;
            vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface, &formatCount, nullptr);
            vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface, &presentCount, nullptr);
            if (formatCount == 0 || presentCount == 0)
                return -1;

            return (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 100 : 10;
        };

        int    bestScore = -1;
        VkPhysicalDevice best = VK_NULL_HANDLE;
        for (auto dev : devices)
        {
            int score = deviceScore(dev);
            if (score > bestScore)
            {
                bestScore = score;
                best      = dev;
            }
        }

        if (best == VK_NULL_HANDLE)
        {
            DT_LOG_ERROR(LogCategory::Renderer,
                "VulkanContext: no suitable GPU found (check device extensions and queue support)");
            return false;
        }

        m_physicalDevice = best;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &props);
        DT_LOG_INFO(LogCategory::Renderer,
            "VulkanContext: selected GPU: {}", props.deviceName);

        // Find queue family indices
        u32 queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, queues.data());

        m_graphicsQueueFamily = UINT32_MAX;
        m_presentQueueFamily  = UINT32_MAX;
        for (u32 i = 0; i < queueCount; ++i)
        {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                m_graphicsQueueFamily = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, surface, &presentSupport);
            if (presentSupport)
                m_presentQueueFamily = i;

            if (m_graphicsQueueFamily != UINT32_MAX && m_presentQueueFamily != UINT32_MAX)
                break;
        }

        return true;
    }

    bool VulkanContext::CreateLogicalDevice(const std::vector<const char*>& deviceExtensions)
    {
        float priority = 1.0f;
        std::set<u32> uniqueFamilies = { m_graphicsQueueFamily, m_presentQueueFamily };

        std::vector<VkDeviceQueueCreateInfo> queueCIs;
        for (u32 family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo qci{};
            qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qci.queueFamilyIndex = family;
            qci.queueCount       = 1;
            qci.pQueuePriorities = &priority;
            queueCIs.push_back(qci);
        }

        VkPhysicalDeviceFeatures features{};
        // No extra features required for M5 quad rendering.

        std::vector<const char*> validationLayers;
        if (m_validationEnabled)
            validationLayers.push_back(kValidationLayerName);

        VkDeviceCreateInfo devCI{};
        devCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devCI.queueCreateInfoCount    = static_cast<u32>(queueCIs.size());
        devCI.pQueueCreateInfos       = queueCIs.data();
        devCI.pEnabledFeatures        = &features;
        devCI.enabledExtensionCount   = static_cast<u32>(deviceExtensions.size());
        devCI.ppEnabledExtensionNames = deviceExtensions.data();
        devCI.enabledLayerCount       = static_cast<u32>(validationLayers.size());
        devCI.ppEnabledLayerNames     = validationLayers.data();

        if (vkCreateDevice(m_physicalDevice, &devCI, nullptr, &m_device) != VK_SUCCESS)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "VulkanContext: vkCreateDevice failed");
            return false;
        }

        vkGetDeviceQueue(m_device, m_graphicsQueueFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, m_presentQueueFamily,  0, &m_presentQueue);
        return true;
    }

    bool VulkanContext::SetupDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT ci{};
        ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        ci.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                           | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        ci.pfnUserCallback = &VulkanContext::DebugCallback;
        ci.pUserData       = nullptr;

        auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!createFn)
            return false;

        return createFn(m_instance, &ci, nullptr, &m_debugMessenger) == VK_SUCCESS;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
        VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
        const VkDebugUtilsMessengerCallbackDataEXT* data,
        void*                                       /*userData*/)
    {
        if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            DT_LOG_ERROR(LogCategory::Renderer, "[Vulkan Validation] {}", data->pMessage);
        }
        else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            DT_LOG_WARN(LogCategory::Renderer, "[Vulkan Validation] {}", data->pMessage);
        }
        return VK_FALSE; // Do not abort the Vulkan call that triggered the message
    }

    // ---------------------------------------------------------------------------

    u32 VulkanContext::FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memProps{};
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);

        for (u32 i = 0; i < memProps.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1u << i)) &&
                (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        DT_ASSERT(false,
            "VulkanContext::FindMemoryType: no matching memory type found");
        return 0; // unreachable in debug; in release this would be UB, but that's
                  // a developer bug (wrong filter), not a recoverable error.
    }

    VkCommandBuffer VulkanContext::BeginOneTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool        = m_oneTimePool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(m_device, &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        return cmd;
    }

    void VulkanContext::EndOneTimeCommands(VkCommandBuffer cmd)
    {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;

        vkQueueSubmit(m_graphicsQueue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphicsQueue);
        vkFreeCommandBuffers(m_device, m_oneTimePool, 1, &cmd);
    }
}
