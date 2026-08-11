#pragma once

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Types.h
//
// Explicit-width primitive aliases used throughout the engine. Two reasons
// to have this instead of using int/long/size_t directly everywhere:
//
// 1. Binary serialization requires exact, platform-independent widths.
//    `long` is 32-bit on Windows and 64-bit on Linux under the same
//    compiler; a save file written on one platform must load correctly on
//    the other, so every serialized field is spelled with an explicit-width
//    type from this header, never a native C++ type directly.
// 2. Self-documentation: `u32 entityCount` communicates range and sign
//    intent at the declaration site without the reader needing to check a
//    header elsewhere.
// ---------------------------------------------------------------------------

namespace dt
{
    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using f32 = float;
    using f64 = double;

    // Explicitly NOT an alias for size_t: usize is always 64-bit regardless
    // of platform, matching the width used in the binary serialization
    // format. size_t itself is still used for raw memory/pointer-arithmetic
    // APIs (new, memcpy, etc.) where matching the standard library's own
    // signature is what's actually required.
    using usize = std::uint64_t;
    using isize = std::int64_t;

    static_assert(sizeof(f32) == 4, "DTEngine requires IEEE-754 32-bit float.");
    static_assert(sizeof(f64) == 8, "DTEngine requires IEEE-754 64-bit double.");
}
