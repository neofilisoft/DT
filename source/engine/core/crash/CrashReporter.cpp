#include "core/crash/CrashReporter.h"
#include "core/logging/Logger.h"
#include "core/platform/BuildConfig.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <sstream>

#if defined(DT_PLATFORM_LINUX)
    #include <execinfo.h>
    #include <signal.h>
    #include <unistd.h>
#elif defined(DT_PLATFORM_WINDOWS)
    #include <Windows.h>
    #include <DbgHelp.h>
    #pragma comment(lib, "Dbghelp.lib")
#endif

namespace dt
{
    namespace
    {
        std::mutex g_breadcrumbMutex;

        u64 NowUnixMs()
        {
            return static_cast<u64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }

    CrashReporter& CrashReporter::Get()
    {
        static CrashReporter instance;
        return instance;
    }

    std::vector<std::string> CrashReporter::CaptureStackTrace()
    {
        std::vector<std::string> frames;

#if defined(DT_PLATFORM_LINUX)
        // backtrace()/backtrace_symbols() are on the async-signal-safe-ish
        // border (glibc's implementation is widely used in signal handlers
        // in practice, though POSIX does not formally guarantee
        // backtrace_symbols is async-signal-safe due to its internal
        // malloc use). This is an accepted, deliberate risk here: on the
        // path where we've already decided the process cannot continue
        // safely (a fault handler about to abort), getting a stack trace
        // most of the time is worth the small risk of the handler itself
        // faulting in the rare case the heap is already corrupted -
        // that failure mode produces no report either way, whereas not
        // attempting the trace guarantees no report.
        constexpr int kMaxFrames = 64;
        void* addresses[kMaxFrames];
        const int frameCount = ::backtrace(addresses, kMaxFrames);
        char** symbols = ::backtrace_symbols(addresses, frameCount);

        if (symbols != nullptr)
        {
            for (int i = 0; i < frameCount; ++i)
            {
                frames.emplace_back(symbols[i]);
            }
            std::free(symbols);
        }
#elif defined(DT_PLATFORM_WINDOWS)
        constexpr int kMaxFrames = 64;
        void* addresses[kMaxFrames];
        HANDLE process = GetCurrentProcess();
        SymInitialize(process, nullptr, TRUE);

        const WORD frameCount = CaptureStackBackTrace(0, kMaxFrames, addresses, nullptr);

        constexpr int kMaxNameLen = 256;
        char symbolBuffer[sizeof(SYMBOL_INFO) + kMaxNameLen];
        SYMBOL_INFO* symbolInfo = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbolInfo->MaxNameLen = kMaxNameLen;
        symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);

        for (WORD i = 0; i < frameCount; ++i)
        {
            const DWORD64 addr = reinterpret_cast<DWORD64>(addresses[i]);
            std::ostringstream line;
            if (SymFromAddr(process, addr, nullptr, symbolInfo))
            {
                line << symbolInfo->Name << " [0x" << std::hex << addr << "]";
            }
            else
            {
                line << "0x" << std::hex << addr << " (no symbol)";
            }
            frames.push_back(line.str());
        }
#endif

        return frames;
    }

    void CrashReporter::AddBreadcrumb(std::string message)
    {
        std::lock_guard<std::mutex> lock(g_breadcrumbMutex);
        m_breadcrumbs.push_back(std::move(message));
        if (m_breadcrumbs.size() > kMaxBreadcrumbs)
        {
            m_breadcrumbs.erase(m_breadcrumbs.begin());
        }
    }

    std::vector<std::string> CrashReporter::GetBreadcrumbsSnapshot() const
    {
        std::lock_guard<std::mutex> lock(g_breadcrumbMutex);
        return m_breadcrumbs;
    }

    void CrashReporter::WriteReport(const CrashReport& report)
    {
        std::ostringstream fileName;
        fileName << m_dumpDirectory << "/crash_" << report.unixTimeMs << ".log";

        // Deliberately uses raw fopen/fprintf, NOT FileSystem::Get() or
        // Logger - both of those may themselves be in an inconsistent
        // state (locked mutex on the crashing thread, heap corruption)
        // when this runs from inside a signal handler. This function must
        // have the absolute minimum dependency surface: libc file I/O
        // only.
        std::FILE* file = std::fopen(fileName.str().c_str(), "w");
        if (file == nullptr)
        {
            return;
        }

        std::fprintf(file, "DTEngine Crash Report\n");
        std::fprintf(file, "Time (unix ms): %llu\n", static_cast<unsigned long long>(report.unixTimeMs));
        std::fprintf(file, "Reason: %s\n", report.reason.c_str());
        std::fprintf(file, "Faulting address: %s\n\n", report.faultingAddress.c_str());

        std::fprintf(file, "--- Stack Trace ---\n");
        for (const std::string& frame : report.stackTrace)
        {
            std::fprintf(file, "  %s\n", frame.c_str());
        }

        std::fprintf(file, "\n--- Breadcrumbs (most recent last) ---\n");
        for (const std::string& crumb : report.breadcrumbs)
        {
            std::fprintf(file, "  %s\n", crumb.c_str());
        }

        std::fprintf(file, "\n--- Recent Log Lines ---\n");
        for (const std::string& line : report.recentLogLines)
        {
            std::fprintf(file, "  %s\n", line.c_str());
        }

        std::fflush(file);
        std::fclose(file);
    }

#if defined(DT_PLATFORM_LINUX)

    namespace
    {
        // Signal handlers must be plain C-linkage-compatible free
        // functions (not member functions) since the OS calls them with a
        // fixed signature. They read from CrashReporter::Get() as file-
        // scope state to reach the installed dump directory / breadcrumbs.
        void SignalHandler(int signum, siginfo_t* info, void* /*context*/)
        {
            CrashReport report;
            report.unixTimeMs = NowUnixMs();

            switch (signum)
            {
                case SIGSEGV: report.reason = "SIGSEGV (segmentation fault)"; break;
                case SIGABRT: report.reason = "SIGABRT (abort)"; break;
                case SIGFPE:  report.reason = "SIGFPE (floating point exception)"; break;
                case SIGILL:  report.reason = "SIGILL (illegal instruction)"; break;
                case SIGBUS:  report.reason = "SIGBUS (bus error)"; break;
                default:      report.reason = "Unknown signal " + std::to_string(signum); break;
            }

            char addrBuf[32];
            std::snprintf(addrBuf, sizeof(addrBuf), "%p", info->si_addr);
            report.faultingAddress = addrBuf;

            report.stackTrace = CrashReporter::CaptureStackTrace();
            report.breadcrumbs = CrashReporter::Get().GetBreadcrumbsSnapshot();

            CrashReporter::Get().WriteReport(report);

            // Re-raise with the default handler so the OS still produces
            // its own core dump / correct exit code - CrashReporter
            // augments the OS's own fault reporting, it does not replace
            // it or attempt to keep the process alive afterward (the
            // process state post-fault cannot be trusted to continue
            // running).
            signal(signum, SIG_DFL);
            raise(signum);
        }
    }

    void CrashReporter::Install(const std::string& crashDumpDirectory)
    {
        m_dumpDirectory = crashDumpDirectory;

        struct sigaction action{};
        action.sa_sigaction = SignalHandler;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);

        sigaction(SIGSEGV, &action, nullptr);
        sigaction(SIGABRT, &action, nullptr);
        sigaction(SIGFPE, &action, nullptr);
        sigaction(SIGILL, &action, nullptr);
        sigaction(SIGBUS, &action, nullptr);

        std::set_terminate([]() {
            CrashReport report;
            report.unixTimeMs = NowUnixMs();
            report.reason = "std::terminate called (unhandled C++ exception or noexcept violation)";
            report.faultingAddress = "n/a";
            report.stackTrace = CrashReporter::CaptureStackTrace();
            report.breadcrumbs = CrashReporter::Get().GetBreadcrumbsSnapshot();
            CrashReporter::Get().WriteReport(report);
            std::abort();
        });

        DT_LOG_INFO(LogCategory::Core, "CrashReporter installed, dumping to '{}'", crashDumpDirectory);
    }

#elif defined(DT_PLATFORM_WINDOWS)

    namespace
    {
        LONG WINAPI VectoredHandler(EXCEPTION_POINTERS* exceptionInfo)
        {
            CrashReport report;
            report.unixTimeMs = NowUnixMs();

            char reasonBuf[64];
            std::snprintf(reasonBuf, sizeof(reasonBuf), "SEH exception code 0x%08lX",
                exceptionInfo->ExceptionRecord->ExceptionCode);
            report.reason = reasonBuf;

            char addrBuf[32];
            std::snprintf(addrBuf, sizeof(addrBuf), "%p", exceptionInfo->ExceptionRecord->ExceptionAddress);
            report.faultingAddress = addrBuf;

            report.stackTrace = CrashReporter::CaptureStackTrace();
            report.breadcrumbs = CrashReporter::Get().GetBreadcrumbsSnapshot();

            CrashReporter::Get().WriteReport(report);

            return EXCEPTION_CONTINUE_SEARCH; // let the OS/debugger still handle it normally afterward
        }
    }

    void CrashReporter::Install(const std::string& crashDumpDirectory)
    {
        m_dumpDirectory = crashDumpDirectory;
        AddVectoredExceptionHandler(1, VectoredHandler);

        std::set_terminate([]() {
            CrashReport report;
            report.unixTimeMs = NowUnixMs();
            report.reason = "std::terminate called (unhandled C++ exception or noexcept violation)";
            report.faultingAddress = "n/a";
            report.stackTrace = CrashReporter::CaptureStackTrace();
            report.breadcrumbs = CrashReporter::Get().GetBreadcrumbsSnapshot();
            CrashReporter::Get().WriteReport(report);
            std::abort();
        });

        DT_LOG_INFO(LogCategory::Core, "CrashReporter installed, dumping to '{}'", crashDumpDirectory);
    }

#endif

    void CrashReporter::SimulateCrashReport(const std::string& reason)
    {
        CrashReport report;
        report.unixTimeMs = NowUnixMs();
        report.reason = "[SIMULATED] " + reason;
        report.faultingAddress = "n/a (simulated)";
        report.stackTrace = CaptureStackTrace();
        report.breadcrumbs = GetBreadcrumbsSnapshot();
        WriteReport(report);
    }
}
