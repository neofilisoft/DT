#pragma once

#include "core/platform/BuildConfig.h"

#include <cstdio>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Assert.h
//
// Two macros with deliberately different guarantees:
//
// DT_ASSERT(cond, msg) - compiled out entirely in Shipping (DT_WITH_ASSERTS
//   == 0). Use for invariant checks where the *condition itself* is only
//   ever evaluated for the check's sake and has no side effect the program
//   depends on. Example: DT_ASSERT(index < count, "index out of range").
//
// DT_VERIFY(expr, msg) - the expression is ALWAYS evaluated, in every
//   configuration, including Shipping. Only the failure-reporting (log +
//   breakpoint) is stripped in Shipping. Use this when `expr` performs a
//   side effect the program correctness depends on, e.g.
//   DT_VERIFY(file.Open(path), "failed to open save file") - if you used
//   DT_ASSERT here, Shipping builds would never call file.Open() at all.
//
// This distinction is the single most common source of "works in Debug,
// broken in Shipping" bugs in engines that only have one assert macro, so
// it is treated as a hard API contract here rather than a convention.
// ---------------------------------------------------------------------------

namespace dt::detail
{
    // Kept out-of-line (not header-inlined as a lambda at every call site)
    // so that adding a crash-reporter hook later (core/crash) only requires
    // editing this one function body, not every assert call site in the
    // codebase.
    void ReportAssertFailure(const char* expr, const char* file, int line, const char* msg);
}

#if DT_WITH_ASSERTS
    #define DT_ASSERT(cond, msg)                                                   \
        do                                                                         \
        {                                                                          \
            if (!(cond))                                                          \
            {                                                                      \
                ::dt::detail::ReportAssertFailure(#cond, __FILE__, __LINE__, msg); \
                DT_DEBUGBREAK();                                                  \
                std::abort();                                                     \
            }                                                                      \
        } while (0)
#else
    #define DT_ASSERT(cond, msg) do { (void)sizeof(cond); } while (0)
#endif

#if DT_WITH_ASSERTS
    #define DT_VERIFY(expr, msg)                                                       \
        [&]() -> bool {                                                                \
            bool dt_verify_result = static_cast<bool>(expr);                          \
            if (!dt_verify_result)                                                     \
            {                                                                          \
                ::dt::detail::ReportAssertFailure(#expr, __FILE__, __LINE__, msg);     \
                DT_DEBUGBREAK();                                                       \
                std::abort();                                                          \
            }                                                                          \
            return dt_verify_result;                                                  \
        }()
#else
    #define DT_VERIFY(expr, msg) (static_cast<bool>(expr))
#endif

#define DT_UNREACHABLE(msg)                                                             \
    do                                                                                  \
    {                                                                                   \
        ::dt::detail::ReportAssertFailure("unreachable", __FILE__, __LINE__, msg);      \
        std::abort();                                                                   \
    } while (0)
