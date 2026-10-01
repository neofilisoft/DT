// Copyright Neofilisoft. All Rights Reserved.
#pragma once

#include "core/platform/Types.h"
#include "core/math/Math.h"
#include "core/reflection/Reflection.h"

namespace lacrima
{
    // Configuration for procedural noise generation, fully reflectable for serialization
    struct NoiseConfig
    {
        u32 seed = 1337;
        f32 frequency = 1.0f;
        f32 amplitude = 1.0f;
        i32 octaves = 1;
        f32 persistence = 0.5f;
        f32 lacunarity = 2.0f;

        REFLECT_BEGIN(NoiseConfig)
            REFLECT_FIELD(seed)
            REFLECT_FIELD(frequency)
            REFLECT_FIELD(amplitude)
            REFLECT_FIELD(octaves)
            REFLECT_FIELD(persistence)
            REFLECT_FIELD(lacunarity)
        REFLECT_END()
    };

    class Procedural
    {
    public:
        // Generates 2D Perlin-like noise using the provided configuration.
        // Returns a value between -1.0f and 1.0f
        static f32 PerlinNoise2D(const Vec2& position, const NoiseConfig& config);
        
        // Simple 1D hash based on integer inputs
        static u32 Hash(u32 x, u32 seed);
        
        // Simple 2D hash based on integer inputs
        static u32 Hash2D(i32 x, i32 y, u32 seed);

    private:
        static f32 SmoothStep(f32 t);
        static f32 Lerp(f32 a, f32 b, f32 t);
        static Vec2 GetGradient(i32 ix, i32 iy, u32 seed);
        static f32 PerlinNoise2DSingle(f32 x, f32 y, u32 seed);
    };
}

