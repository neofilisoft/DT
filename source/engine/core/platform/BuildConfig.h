#pragma once

// ---------------------------------------------------------------------------
// BuildConfig.h
//
// Single source of truth for compile-time configuration switches. Every
// other Core header includes this instead of re-deriving DT_DEBUG /
// DT_SHIPPING logic locally, so there is exactly one place that defines what
// "Shipping" means.
//
// CMakeLists.txt defines exactly one of DT_DEBUG / DT_RELEASE / DT_SHIPPING
// via add_compile_definitions per-config generator expressions. This header
// normalizes that into the feature-flag macros consumed throughout Core.
// ---------------------------------------------------------------------------

#if !defined(DT_DEBUG) && !defined(DT_RELEASE) && !defined(DT_SHIPPING)
    #error "DTEngine: no build configuration defined. Expected exactly one of DT_DEBUG, DT_RELEASE, DT_SHIPPING from CMake."
#endif

// --- Platform ---------------------------------------------------------------

#if defined(DT_PLATFORM_WINDOWS)
    #define DT_PLATFORM_NAME "Windows"
#elif defined(DT_PLATFORM_LINUX)
    #define DT_PLATFORM_NAME "Linux"
#else
    #error "DTEngine: no platform defined."
#endif

// --- Compiler -----------------------------------------------------------

#if defined(_MSC_VER)
    #define DT_COMPILER_MSVC 1
    #define DT_FORCEINLINE __forceinline
    #define DT_DEBUGBREAK() __debugbreak()
#elif defined(__clang__)
    #define DT_COMPILER_CLANG 1
    #define DT_FORCEINLINE inline __attribute__((always_inline))
    #define DT_DEBUGBREAK() __builtin_debugtrap()
#elif defined(__GNUC__)
    #define DT_COMPILER_GCC 1
    #define DT_FORCEINLINE inline __attribute__((always_inline))
    #define DT_DEBUGBREAK() __builtin_trap()
#else
    #error "DTEngine: unsupported compiler."
#endif

// --- Feature flags ------------------------------------------------------
//
// DT_WITH_MEMORY_TRACKING : per-allocation bookkeeping (call site, size,
//   category) in MemoryTracker. Costs a hash map insert per allocation, so
//   it is compiled out entirely in Shipping rather than merely disabled at
//   runtime - the goal is zero overhead, not "off by default".
//
// DT_WITH_PROFILER        : Profiler scope capture (see core/profiler).
//   Shipping keeps the macros as no-ops so instrumented code doesn't need
//   #ifdef guards at every call site.
//
// DT_WITH_ASSERTS          : DT_ASSERT expands to a real check + breakpoint
//   in Debug/Release, and to nothing in Shipping. Shipping still gets
//   DT_VERIFY (see Assert.h) for checks whose side effect must run.
//
// DT_WITH_REFLECTION_NAMES : keep human-readable type/field name strings in
//   TypeInfo. Shipping keeps only the stable 64-bit hash of each name, since
//   string tables meaningfully bloat the binary and are a diagnostic-only
//   convenience.

#if defined(DT_SHIPPING)
    #define DT_WITH_MEMORY_TRACKING 0
    #define DT_WITH_PROFILER 0
    #define DT_WITH_ASSERTS 0
    #define DT_WITH_REFLECTION_NAMES 0
#else
    #define DT_WITH_MEMORY_TRACKING 1
    #define DT_WITH_PROFILER 1
    #define DT_WITH_ASSERTS 1
    #define DT_WITH_REFLECTION_NAMES 1
#endif
