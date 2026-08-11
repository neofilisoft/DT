#include "core/platform/Assert.h"

namespace dt::detail
{
    void ReportAssertFailure(const char* expr, const char* file, int line, const char* msg)
    {
        // Deliberately uses stderr + fprintf directly rather than the Logger
        // (core/logging). Logger initialization itself is guarded by
        // DT_ASSERT in a few paths, and Logger's own formatting path could
        // in principle be the thing that's broken when an assert fires. A
        // fault reporter must not depend on the subsystem it might be
        // reporting a fault in.
        std::fprintf(
            stderr,
            "[DTEngine] Assertion failed: %s\n  File: %s:%d\n  Message: %s\n",
            expr,
            file,
            line,
            msg ? msg : "(no message)"
        );
        std::fflush(stderr);
    }
}
