// Copyright Neofilisoft. All Rights Reserved.
#include "Procedural.h"
#include <cmath>

namespace lacrima
{
    u32 Procedural::Hash(u32 x, u32 seed)
    {
        x ^= seed;
        x ^= x >> 16;
        x *= 0x85ebca6b;
        x ^= x >> 13;
        x *= 0xc2b2ae35;
        x ^= x >> 16;
        return x;
    }

    u32 Procedural::Hash2D(i32 x, i32 y, u32 seed)
    {
        // Simple hash combining x, y and seed
        u32 h1 = Hash(static_cast<u32>(x), seed);
        return Hash(static_cast<u32>(y), h1);
    }

    f32 Procedural::SmoothStep(f32 t)
    {
        return t * t * (3.0f - 2.0f * t);
    }

    f32 Procedural::Lerp(f32 a, f32 b, f32 t)
    {
        return a + t * (b - a);
    }

    Vec2 Procedural::GetGradient(i32 ix, i32 iy, u32 seed)
    {
        // Generate pseudo-random angle
        u32 random = Hash2D(ix, iy, seed);
        // Map to [0, 2pi]
        f32 angle = static_cast<f32>(random) / static_cast<f32>(0xFFFFFFFF) * 2.0f * 3.14159265f;
        return Vec2(std::cos(angle), std::sin(angle));
    }

    f32 Procedural::PerlinNoise2DSingle(f32 x, f32 y, u32 seed)
    {
        // Grid cell coordinates
        i32 ix0 = static_cast<i32>(std::floor(x));
        i32 iy0 = static_cast<i32>(std::floor(y));
        i32 ix1 = ix0 + 1;
        i32 iy1 = iy0 + 1;

        // Relative x, y inside cell
        f32 fx0 = x - static_cast<f32>(ix0);
        f32 fy0 = y - static_cast<f32>(iy0);
        f32 fx1 = fx0 - 1.0f;
        f32 fy1 = fy0 - 1.0f;

        // Gradients at 4 corners
        Vec2 g00 = GetGradient(ix0, iy0, seed);
        Vec2 g10 = GetGradient(ix1, iy0, seed);
        Vec2 g01 = GetGradient(ix0, iy1, seed);
        Vec2 g11 = GetGradient(ix1, iy1, seed);

        // Dot products
        f32 dp00 = g00.x * fx0 + g00.y * fy0;
        f32 dp10 = g10.x * fx1 + g10.y * fy0;
        f32 dp01 = g01.x * fx0 + g01.y * fy1;
        f32 dp11 = g11.x * fx1 + g11.y * fy1;

        // Smoothstep weights
        f32 wx = SmoothStep(fx0);
        f32 wy = SmoothStep(fy0);

        // Interpolate
        f32 top = Lerp(dp00, dp10, wx);
        f32 bottom = Lerp(dp01, dp11, wx);
        
        return Lerp(top, bottom, wy);
    }

    f32 Procedural::PerlinNoise2D(const Vec2& position, const NoiseConfig& config)
    {
        f32 total = 0.0f;
        f32 max_value = 0.0f;
        f32 current_amplitude = config.amplitude;
        f32 current_frequency = config.frequency;

        for (i32 i = 0; i < config.octaves; ++i)
        {
            total += PerlinNoise2DSingle(position.x * current_frequency, position.y * current_frequency, config.seed + i) * current_amplitude;
            max_value += current_amplitude;
            
            current_amplitude *= config.persistence;
            current_frequency *= config.lacunarity;
        }

        if (max_value > 0.0f)
        {
            return total / max_value;
        }
        
        return 0.0f;
    }
}

