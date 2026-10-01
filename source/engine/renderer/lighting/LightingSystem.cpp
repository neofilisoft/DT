// Copyright Neofilisoft. All Rights Reserved.
#include "renderer/lighting/LightingSystem.h"
#include "renderer/vulkan/VulkanContext.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "core/logging/Logger.h"

namespace lacrima::renderer
{
    bool LightingSystem::Initialize(VulkanContext& ctx)
    {
        LACRIMA_LOG_INFO(LogCategory::Renderer, "LightingSystem initialized");
        m_lightData.pointLightCount = 0;
        m_lightData.spotLightCount = 0;
        
        if (!m_lightBuffer.Initialize(ctx, sizeof(LightDataUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            LACRIMA_LOG_ERROR(LogCategory::Renderer, "Failed to initialize LightBuffer");
            return false;
        }
        m_lightBuffer.Map(ctx);
        
        m_isDirty = true;
        return true;
    }

    void LightingSystem::Shutdown(VulkanContext& ctx)
    {
        if (m_lightBuffer.Mapped()) {
            m_lightBuffer.Unmap(ctx);
        }
        m_lightBuffer.Shutdown(ctx);
        LACRIMA_LOG_INFO(LogCategory::Renderer, "LightingSystem shut down");
    }

    void LightingSystem::SetDirectionalLight(const DirectionalLight& light)
    {
        m_lightData.dirLight = light;
        m_isDirty = true;
    }

    void LightingSystem::AddPointLight(const PointLight& light)
    {
        if (m_lightData.pointLightCount < 16)
        {
            m_lightData.pointLights[m_lightData.pointLightCount++] = light;
            m_isDirty = true;
        }
        else
        {
            LACRIMA_LOG_WARN(LogCategory::Renderer, "LightingSystem: Maximum point lights (16) reached");
        }
    }

    void LightingSystem::AddSpotLight(const SpotLight& light)
    {
        if (m_lightData.spotLightCount < 16)
        {
            m_lightData.spotLights[m_lightData.spotLightCount++] = light;
            m_isDirty = true;
        }
        else
        {
            LACRIMA_LOG_WARN(LogCategory::Renderer, "LightingSystem: Maximum spot lights (16) reached");
        }
    }

    void LightingSystem::ClearLights()
    {
        m_lightData.pointLightCount = 0;
        m_lightData.spotLightCount = 0;
        m_isDirty = true;
    }

    void LightingSystem::UpdateGPUData(VulkanContext& ctx)
    {
        if (m_isDirty && m_lightBuffer.Mapped())
        {
            memcpy(m_lightBuffer.Mapped(), &m_lightData, sizeof(LightDataUBO));
            m_isDirty = false;
        }
    }
}


