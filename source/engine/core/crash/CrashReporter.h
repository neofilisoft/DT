#pragma once

#include "core/platform/Types.h"

#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// CrashReporter.h
//
// Installs process-wide fault handlers (SIGSEGV/SIGABRT/SIGFPE/SIGILL on
// Linux, vectored exception handler + std::set_terminate on Windows) that
// write a crash report to disk before the process dies. This is separate
// from Assert.h/DT_ASSERT: an assert is a caught, expected-to-be-caught
// programming error with a controlled response (log + breakpoint in
// Debug). CrashReporter exists for the *uncaught* case - a genuine
// segfault, an unhandled C++ exception escaping main, a stack overflow -
// where the goal is purely to get as much diagnostic context onto disk as
// possible before the OS terminates the process, since at that point
// there is no guarantee any further engine code can run safely (the
// fault may have corrupted the heap, a mutex may be permanently locked by
// the crashing thread, etc).
//
// What gets captured:
//   - signal/exception type and faulting instruction address
//   - a symbol-resolved stack trace (best-effort; symbol resolution
//     quality depends on whether the binary has debug info available,
//     which Shipping builds strip - Shipping crash reports include raw
//     addresses only, resolved offline against a retained .pdb/.debug
//     symbol archive from that build)
//   - the last N log lines buffered by Logger (a ring buffer feed
//     specifically for this purpose - see RegisterLogRingBuffer below)
//   - a caller-supplied "breadcrumb" list of recent high-level engine
//     events (e.g. "Tick 4821 started", "Loading save slot 3") registered
//     via AddBreadcrumb, giving crash reports gameplay-level context a raw
//     stack trace alone can't provide
//
// Threading: fault handlers, by their nature, may run on the faulting
// thread while other threads (JobSystem workers) are still executing. The
// handler makes no attempt to pause other threads - on a genuine crash,
// correctness of the crash-writing path matters more than a perfectly
// consistent snapshot of concurrent state, and pausing every worker
// thread from within a signal handler risks the handler itself deadlocking
// (most synchronization primitives are not async-signal-safe).
// ---------------------------------------------------------------------------

namespace dt
{
    struct CrashReport
    {
        std::string reason;               // e.g. "SIGSEGV", "unhandled std::exception: <what()>"
        std::string faultingAddress;
        std::vector<std::string> stackTrace;
        std::vector<std::string> recentLogLines;
        std::vector<std::string> breadcrumbs;
        u64 unixTimeMs = 0;
    };

    class CrashReporter
    {
    public:
        static CrashReporter& Get();

        // Installs platform fault handlers. Call once at engine startup,
        // as early as possible (before JobSystem::Initialize, so worker
        // threads inherit the process-wide handler state correctly on
        // platforms where that matters).
        void Install(const std::string& crashDumpDirectory);

        // Records a lightweight breadcrumb (bounded ring buffer, default
        // capacity 64 - see .cpp) for inclusion in any future crash report.
        // Called from high-level engine/game code at meaningful
        // checkpoints, NOT from hot inner loops (each call takes a lock
        // to append safely from any thread).
        void AddBreadcrumb(std::string message);
        std::vector<std::string> GetBreadcrumbsSnapshot() const;

        // Test/tooling hook: deliberately triggers the crash-report-writing
        // path without actually crashing the process, so the Editor's
        // "Test Crash Reporter" debug menu item can verify the pipeline
        // end-to-end without needing to actually segfault the editor.
        void SimulateCrashReport(const std::string& reason);

        // Public rather than private: the platform fault-handler entry
        // points (SignalHandler on Linux, VectoredHandler on Windows, see
        // CrashReporter.cpp) are necessarily free functions - the OS calls
        // them with a fixed C-linkage-compatible signature, so they cannot
        // be private member functions - and they need to call these two
        // during the actual fault-handling path.
        void WriteReport(const CrashReport& report);
        static std::vector<std::string> CaptureStackTrace();

    private:        std::string m_dumpDirectory;
        std::vector<std::string> m_breadcrumbs;
        static constexpr usize kMaxBreadcrumbs = 64;
    };
}
