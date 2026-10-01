// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include <glm/glm.hpp>

namespace lacrima::renderer
{
    // Alignment is carefully chosen to match HLSL 16-byte alignment rules (float4 boundary)

    struct DirectionalLight
    {
        glm::vec4 direction; // xyz = direction, w = reserved
        glm::vec4 color;     // xyz = color, w = intensity
    };
    static_assert(sizeof(DirectionalLight) == 32, "DirectionalLight size must match HLSL alignment");

    struct PointLight
    {
        glm::vec4 position;       // xyz = position, w = radius
        glm::vec4 color;          // xyz = color, w = intensity
        glm::vec4 attenuation;    // x = constant, y = linear, z = quadratic, w = reserved
    };
    static_assert(sizeof(PointLight) == 48, "PointLight size must match HLSL alignment");

    struct SpotLight
    {
        glm::vec4 position;       // xyz = position, w = radius
        glm::vec4 direction;      // xyz = direction, w = intensity
        glm::vec4 color;          // xyz = color, w = innerConeCos
        glm::vec4 params;         // x = outerConeCos, y = constant, z = linear, w = quadratic
    };
    static_assert(sizeof(SpotLight) == 64, "SpotLight size must match HLSL alignment");

    // Unified Light Data Structure for the GPU
    struct LightDataUBO
    {
        DirectionalLight dirLight;
        
        u32 pointLightCount;
        u32 spotLightCount;
        u32 padding[2];

        // Max lights for UBO approach (if using SSBO, these can be unbounded)
        PointLight pointLights[16];
        SpotLight spotLights[16];
    };
}

