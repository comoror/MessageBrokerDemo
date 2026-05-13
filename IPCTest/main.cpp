#include <windows.h>
#include <stdio.h>
#include "test_framework.h"

int main(int argc, char* argv[])
{
    // Enable CRT memory leak detection in Debug builds
#ifdef _DEBUG
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
#endif

    printf("============================================\n");
    printf("  IPCTest - IPC Library Automated Tests\n");
    printf("============================================\n");

    // Optional: filter by suite name from command line
    const char* filterSuite = nullptr;
    if (argc > 1)
    {
        filterSuite = argv[1];
        printf("  Filter: suite = %s\n", filterSuite);
    }

    TestSummary summary = RunAllTests(filterSuite);

    printf("\n");
    return (summary.failed == 0) ? 0 : 1;
}
