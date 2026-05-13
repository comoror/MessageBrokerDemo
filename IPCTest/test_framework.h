#pragma once

#include <windows.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <algorithm>

// ============================================================
// Assertion Macros
// ============================================================

#define TEST_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            printf("          ^ ASSERTION FAILED: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_EQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a != _b) { \
            printf("          ^ ASSERTION FAILED: %s == %s (got %lld vs %lld) (%s:%d)\n", \
                #a, #b, (long long)_a, (long long)_b, __FILE__, __LINE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_NEQ(a, b) \
    do { \
        auto _a = (a); auto _b = (b); \
        if (_a == _b) { \
            printf("          ^ ASSERTION FAILED: %s != %s (both %lld) (%s:%d)\n", \
                #a, #b, (long long)_a, __FILE__, __LINE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_NOT_NULL(ptr) \
    do { \
        if ((ptr) == nullptr) { \
            printf("          ^ ASSERTION FAILED: %s != nullptr (%s:%d)\n", #ptr, __FILE__, __LINE__); \
            return false; \
        } \
    } while(0)

#define TEST_ASSERT_NULL(ptr) \
    do { \
        if ((ptr) != nullptr) { \
            printf("          ^ ASSERTION FAILED: %s == nullptr (%s:%d)\n", #ptr, __FILE__, __LINE__); \
            return false; \
        } \
    } while(0)

// ============================================================
// PerfTimer - High-resolution timer
// ============================================================

class PerfTimer
{
public:
    PerfTimer() { QueryPerformanceFrequency(&m_freq); }

    void Start() { QueryPerformanceCounter(&m_start); }

    double ElapsedMicroseconds() const
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return (double)(now.QuadPart - m_start.QuadPart) * 1000000.0 / (double)m_freq.QuadPart;
    }

    double ElapsedMilliseconds() const { return ElapsedMicroseconds() / 1000.0; }

private:
    LARGE_INTEGER m_freq = {};
    LARGE_INTEGER m_start = {};
};

// ============================================================
// WaitableFlag - Manual-reset event wrapper
// ============================================================

class WaitableFlag
{
public:
    WaitableFlag() { m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); }
    ~WaitableFlag() { if (m_hEvent) CloseHandle(m_hEvent); }

    void Set() { SetEvent(m_hEvent); }
    void Reset() { ResetEvent(m_hEvent); }
    bool Wait(DWORD timeoutMs = 2000) { return WaitForSingleObject(m_hEvent, timeoutMs) == WAIT_OBJECT_0; }

    WaitableFlag(const WaitableFlag&) = delete;
    WaitableFlag& operator=(const WaitableFlag&) = delete;

private:
    HANDLE m_hEvent = NULL;
};

// ============================================================
// Pipe Name Generator
// ============================================================

inline std::string GetTestPipeName(const char* suite, int index)
{
    char buf[256];
    sprintf_s(buf, "\\\\.\\pipe\\IPCTest_%s_%d_%lu", suite, index, GetCurrentProcessId());
    return std::string(buf);
}

// ============================================================
// Test Registry
// ============================================================

struct TestEntry
{
    const char* suite;
    const char* name;
    std::function<bool()> func;
};

inline std::vector<TestEntry>& GetTestRegistry()
{
    static std::vector<TestEntry> registry;
    return registry;
}

struct TestRegistrar
{
    TestRegistrar(const char* suite, const char* name, std::function<bool()> func)
    {
        GetTestRegistry().push_back({ suite, name, std::move(func) });
    }
};

#define REGISTER_TEST(suite, name, func) \
    static TestRegistrar _reg_##suite##_##name(#suite, #name, func)

// ============================================================
// Test Runner
// ============================================================

struct TestSummary
{
    int total = 0;
    int passed = 0;
    int failed = 0;
};

inline TestSummary RunAllTests(const char* filterSuite = nullptr)
{
    TestSummary summary;
    PerfTimer totalTimer;
    totalTimer.Start();

    const char* currentSuite = nullptr;

    for (auto& entry : GetTestRegistry())
    {
        if (filterSuite && strcmp(entry.suite, filterSuite) != 0)
            continue;

        if (!currentSuite || strcmp(currentSuite, entry.suite) != 0)
        {
            currentSuite = entry.suite;
            printf("\n[SUITE: %s]\n", currentSuite);
        }

        PerfTimer testTimer;
        testTimer.Start();

        bool passed = false;
        __try
        {
            passed = entry.func();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            printf("          ^ EXCEPTION: 0x%08X\n", GetExceptionCode());
            passed = false;
        }

        double elapsedMs = testTimer.ElapsedMilliseconds();

        summary.total++;
        if (passed)
        {
            summary.passed++;
            printf("  [PASS]  %-40s (%6.1f ms)\n", entry.name, elapsedMs);
        }
        else
        {
            summary.failed++;
            printf("  [FAIL]  %-40s (%6.1f ms)\n", entry.name, elapsedMs);
        }
    }

    double totalMs = totalTimer.ElapsedMilliseconds();
    printf("\n=== SUMMARY ===\n");
    printf("Total: %d | Passed: %d | Failed: %d | Time: %.1fs\n",
        summary.total, summary.passed, summary.failed, totalMs / 1000.0);

    return summary;
}

// ============================================================
// Perf Reporting Helper
// ============================================================

inline void ReportThroughput(int msgCount, double elapsedUs)
{
    double seconds = elapsedUs / 1000000.0;
    double msgPerSec = msgCount / seconds;
    double avgLatencyUs = elapsedUs / msgCount;
    printf("          Throughput: %.0f msg/sec | Avg: %.1f us\n", msgPerSec, avgLatencyUs);
}

inline void ReportLatencyPercentiles(std::vector<double>& latenciesUs)
{
    if (latenciesUs.empty()) return;
    std::sort(latenciesUs.begin(), latenciesUs.end());
    size_t n = latenciesUs.size();
    double avg = 0;
    for (auto v : latenciesUs) avg += v;
    avg /= n;
    printf("          Latency: Avg=%.1f us | P50=%.1f us | P99=%.1f us | Min=%.1f us | Max=%.1f us\n",
        avg, latenciesUs[n / 2], latenciesUs[n * 99 / 100], latenciesUs[0], latenciesUs[n - 1]);
}

// ============================================================
// Handle Leak Detection Helper
// ============================================================

inline DWORD GetCurrentHandleCount()
{
    DWORD count = 0;
    GetProcessHandleCount(GetCurrentProcess(), &count);
    return count;
}

// Tolerance: allow up to 'tolerance' handle difference (OS jitter, thread pool, etc.)
#define TEST_ASSERT_NO_HANDLE_LEAK(before, after, tolerance) \
    do { \
        if ((after) > (before) + (tolerance)) { \
            printf("          ^ HANDLE LEAK: before=%lu, after=%lu, leaked=%lu (%s:%d)\n", \
                (unsigned long)(before), (unsigned long)(after), \
                (unsigned long)((after) - (before)), __FILE__, __LINE__); \
            return false; \
        } \
    } while(0)
