// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include "core/reflection/Reflection.h"

namespace lacrima::sim
{
    struct SpringArmComponent
    {
        f32 targetArmLength = 4.0f;       // Desired distance from character
        f32 currentArmLength = 4.0f;      // Actual distance (shortened dynamically on obstacle collision)
        f32 minArmLength = 0.5f;          // Minimum distance so camera doesn't invert
        f32 maxArmLength = 12.0f;         // Maximum zoom distance

        f32 probeRadius = 0.25f;          // SphereCast probe radius (meters)
        f32 collisionPadding = 0.1f;      // Extra safety margin from hit surface

        f32 targetPitch = 0.3f;           // Desired pitch in radians
        f32 targetYaw = 0.0f;             // Desired yaw in radians
        f32 currentPitch = 0.3f;          // Smoothed current pitch
        f32 currentYaw = 0.0f;            // Smoothed current yaw

        f32 cameraLagSpeed = 10.0f;       // Smoothing responsiveness (higher = snappier)
        bool enableCameraLag = true;
        bool doCollisionTest = true;

        Vec3 targetOffset{0.0f, 1.6f, 0.0f}; // Pivot offset from character root (e.g. eye height)

        Vec3 computedCameraPosition{0.0f, 1.6f, 4.0f}; // Final camera position in world space
        Vec3 computedLookAtTarget{0.0f, 1.6f, 0.0f};   // Final target position in world space

        REFLECT_BEGIN(SpringArmComponent)
            REFLECT_FIELD(targetArmLength)
            REFLECT_FIELD(currentArmLength)
            REFLECT_FIELD(minArmLength)
            REFLECT_FIELD(maxArmLength)
            REFLECT_FIELD(probeRadius)
            REFLECT_FIELD(targetPitch)
            REFLECT_FIELD(targetYaw)
            REFLECT_FIELD(cameraLagSpeed)
            REFLECT_FIELD(enableCameraLag)
            REFLECT_FIELD(doCollisionTest)
        REFLECT_END()
    };
}