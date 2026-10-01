// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/math/Math.h"

namespace lacrima::renderer
{
    // Maximum number of CSM cascades.
    // Must match kCSMCascadeCount in CSMShadowPass.h.
    static constexpr uint32_t kMaxCSMCascades = 4;

    // Global uniforms sent to every shader via Descriptor Set 0.
    // std140 layout: every mat4 is 64 bytes, vec4 is 16 bytes.
    // The shadow fields are ONLY used by shaders that sample shadow maps
    // (static_mesh.frag, skinned_mesh.frag). Pure 2D shaders ignore them
    // at zero GPU overhead because they are part of the same UBO.
    struct GlobalUniforms
    {
        Mat4 viewMatrix;
        Mat4 projectionMatrix;
        Mat4 viewProjectionMatrix;

        // Basic lighting
        Vec4 lightDirection; // w = 0.0f (direction TO light, negated sun dir)
        Vec4 lightColor;     // RGB + Intensity (w)
        Vec4 ambientColor;   // RGB + Intensity (w)

        // Camera position for specular calculations
        Vec4 cameraPosition; // xyz, w = 1.0f

        // --- Cascaded Shadow Map (CSM) ---
        // lightSpaceMatrices[i] = lightProj[i] * lightView
        // Transforms world position into NDC space of cascade i.
        Mat4 lightSpaceMatrices[kMaxCSMCascades];

        // View-space Z depths at which cascade i ends.
        // cascadeSplitDepths[i].x = far plane of cascade i (view space, negative Z forward).
        // Compared against the fragment's gl_FragCoord.z after linearisation.
        Vec4 cascadeSplitDepths; // x=cascade0, y=cascade1, z=cascade2, w=cascade3
    };
}
