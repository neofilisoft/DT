// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "renderer/lighting/LightTypes.h"
#include <vector>

#include "renderer/vulkan/VulkanBuffer.h"

namespace lacrima::renderer
{
    class VulkanContext;

    class LightingSystem
    {
    public:
        LightingSystem() = default;
        ~LightingSystem() = default;

        // Non-copyable, non-movable
        LightingSystem(const LightingSystem&) = delete;
        LightingSystem& operator=(const LightingSystem&) = delete;

        bool Initialize(VulkanContext& ctx);
        void Shutdown(VulkanContext& ctx);

        void SetDirectionalLight(const DirectionalLight& light);
        void AddPointLight(const PointLight& light);
        void AddSpotLight(const SpotLight& light);

        void ClearLights();

        // Call before rendering to update GPU buffers
        void UpdateGPUData(VulkanContext& ctx);

        // Accessors
        const LightDataUBO& GetLightData() const { return m_lightData; }
        
        // Return VulkanBuffer handle to bind to descriptor sets
        VkBuffer GetLightBufferHandle() const { return m_lightBuffer.Handle(); }

    private:
        LightDataUBO m_lightData{};
        VulkanBuffer m_lightBuffer;
        bool m_isDirty = true;
    };
}

