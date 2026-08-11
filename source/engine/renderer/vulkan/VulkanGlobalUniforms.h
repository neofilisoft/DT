#pragma once

#include "core/math/Math.h"

namespace dt::renderer
{
    // Global uniforms sent to every shader via Descriptor Set 0
    struct GlobalUniforms
    {
        Mat4 viewMatrix;
        Mat4 projectionMatrix;
        Mat4 viewProjectionMatrix;

        // Basic lighting
        Vec4 lightDirection; // w = 0.0f
        Vec4 lightColor;     // RGB + Intensity (w)
        Vec4 ambientColor;   // RGB + Intensity (w)

        // Camera position for specular calculations if needed
        Vec4 cameraPosition; // xyz, w = 1.0f
    };
}
