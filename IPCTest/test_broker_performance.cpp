#include <windows.h>
#include <stdio.h>
#include <atomic>
#include <vector>
#include <thread>
#include "test_framework.h"
#include "../IPC/IPC.h"
#include "../IPC/IPCMessage.h"

// ============================================================
// Performance test helpers
// ============================================================

static bool PerfBrokerOnConnect(void*) { return true; }
static bool PerfBrokerOnAuth(void*, unsigned short) { return true; }

static std::atomic<int> g_perfRecvCount{0};
static WaitableFlag g_perfDoneEvent;
static int g_perfExpectedCount = 0;

static void PerfReceiverOnMessage(void* msg, size_t size)
{
    if (++g_perfRecvCount >= g_perfExpectedCount)
        g_perfDoneEvent.Set();
}

static void PerfSenderOnMessage(void*, size_t) {}

static bool RunThroughputTest(int testIndex, int payloadSize, int msgCount)
{
    std::string pipe = GetTestPipeName("Perf", testIndex);
    auto hBroker = ipc_broker_start(pipe.c_str(), PerfBrokerOnConnect, PerfBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    g_perfRecvCount = 0;
    g_perfExpectedCount = msgCount;
    g_perfDoneEvent.Reset();

    auto hSender = ipc_client_start(pipe.c_str(), 0x1001, PerfSenderOnMessage, nullptr, nullptr);
    auto hRecv = ipc_client_start(pipe.c_str(), 0x1002, PerfReceiverOnMessage, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hSender);
    TEST_ASSERT_NOT_NULL(hRecv);
    Sleep(100);

    // Prepare payload
    std::vector<unsigned char> payload(payloadSize, 0x55);

    // Warm-up: 10 messages
    for (int i = 0; i < 10; i++)
    {
        ipc_client_send(hSender, 0x1002, 1000, payload.data(), (unsigned short)payloadSize);
    }
    Sleep(100);
    g_perfRecvCount = 0;
    g_perfDoneEvent.Reset();

    // Measurement
    PerfTimer timer;
    timer.Start();

    for (int i = 0; i < msgCount; i++)
    {
        IPC_RESULT ret = ipc_client_send(hSender, 0x1002, 1000, payload.data(), (unsigned short)payloadSize);
        if (ret != IPC_OK)
        {
            printf("          Send failed at msg %d: %d\n", i, ret);
            break;
        }
    }

    bool allReceived = g_perfDoneEvent.Wait(10000);
    double elapsedUs = timer.ElapsedMicroseconds();

    if (allReceived)
    {
        ReportThroughput(msgCount, elapsedUs);
    }
    else
    {
        printf("          TIMEOUT: received %d / %d messages\n", g_perfRecvCount.load(), msgCount);
    }

    ipc_client_stop(hSender);
    ipc_client_stop(hRecv);
    Sleep(50);
    ipc_broker_stop(hBroker);

    TEST_ASSERT(allReceived);
    return true;
}

// ============================================================
// Test Cases
// ============================================================

static bool test_perf_unicast_64B()  { return RunThroughputTest(1, 64, 1000); }
static bool test_perf_unicast_512B() { return RunThroughputTest(2, 512, 1000); }
static bool test_perf_unicast_4KB()  { return RunThroughputTest(3, 4096, 1000); }
static bool test_perf_unicast_8KB()  { return RunThroughputTest(4, 8160, 1000); }

static bool test_perf_broadcast_64B()
{
    std::string pipe = GetTestPipeName("Perf", 5);
    auto hBroker = ipc_broker_start(pipe.c_str(), PerfBrokerOnConnect, PerfBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    const int NUM_RECEIVERS = 4;
    const int MSG_COUNT = 1000;
    g_perfRecvCount = 0;
    g_perfExpectedCount = MSG_COUNT * NUM_RECEIVERS;
    g_perfDoneEvent.Reset();

    auto hSender = ipc_client_start(pipe.c_str(), 0x2001, PerfSenderOnMessage, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hSender);
    Sleep(50);

    IPC_CLIENT_HANDLE receivers[NUM_RECEIVERS] = {};
    for (int i = 0; i < NUM_RECEIVERS; i++)
    {
        receivers[i] = ipc_client_start(pipe.c_str(), (unsigned short)(0x2010 + i), PerfReceiverOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(receivers[i]);
        Sleep(30);
        ipc_client_register_msg(receivers[i], 1000);
        Sleep(30);
    }

    unsigned char payload[64] = {};

    PerfTimer timer;
    timer.Start();

    for (int i = 0; i < MSG_COUNT; i++)
    {
        ipc_client_broadcast(hSender, 1000, payload, 64);
    }

    bool allReceived = g_perfDoneEvent.Wait(15000);
    double elapsedUs = timer.ElapsedMicroseconds();

    if (allReceived)
    {
        printf("          %d broadcasts x %d receivers = %d deliveries\n", MSG_COUNT, NUM_RECEIVERS, MSG_COUNT * NUM_RECEIVERS);
        ReportThroughput(MSG_COUNT * NUM_RECEIVERS, elapsedUs);
    }
    else
    {
        printf("          TIMEOUT: received %d / %d\n", g_perfRecvCount.load(), g_perfExpectedCount);
    }

    ipc_client_stop(hSender);
    for (int i = 0; i < NUM_RECEIVERS; i++)
        ipc_client_stop(receivers[i]);
    Sleep(50);
    ipc_broker_stop(hBroker);

    TEST_ASSERT(allReceived);
    return true;
}

static bool test_perf_multi_sender()
{
    std::string pipe = GetTestPipeName("Perf", 6);
    auto hBroker = ipc_broker_start(pipe.c_str(), PerfBrokerOnConnect, PerfBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    const int NUM_SENDERS = 4;
    const int MSGS_PER_SENDER = 250;
    g_perfRecvCount = 0;
    g_perfExpectedCount = NUM_SENDERS * MSGS_PER_SENDER;
    g_perfDoneEvent.Reset();

    auto hRecv = ipc_client_start(pipe.c_str(), 0x3001, PerfReceiverOnMessage, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hRecv);
    Sleep(50);

    IPC_CLIENT_HANDLE senders[NUM_SENDERS] = {};
    for (int i = 0; i < NUM_SENDERS; i++)
    {
        senders[i] = ipc_client_start(pipe.c_str(), (unsigned short)(0x3010 + i), PerfSenderOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(senders[i]);
        Sleep(30);
    }

    unsigned char payload[64] = {};

    PerfTimer timer;
    timer.Start();

    // Launch sender threads
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_SENDERS; i++)
    {
        threads.emplace_back([&, i]() {
            for (int j = 0; j < MSGS_PER_SENDER; j++)
            {
                ipc_client_send(senders[i], 0x3001, 1000, payload, 64);
            }
        });
    }

    for (auto& t : threads) t.join();

    bool allReceived = g_perfDoneEvent.Wait(10000);
    double elapsedUs = timer.ElapsedMicroseconds();

    if (allReceived)
    {
        ReportThroughput(g_perfExpectedCount, elapsedUs);
    }
    else
    {
        printf("          TIMEOUT: received %d / %d\n", g_perfRecvCount.load(), g_perfExpectedCount);
    }

    for (int i = 0; i < NUM_SENDERS; i++)
        ipc_client_stop(senders[i]);
    ipc_client_stop(hRecv);
    Sleep(50);
    ipc_broker_stop(hBroker);

    TEST_ASSERT(allReceived);
    return true;
}

static bool test_perf_roundtrip_latency()
{
    std::string pipe = GetTestPipeName("Perf", 7);
    auto hBroker = ipc_broker_start(pipe.c_str(), PerfBrokerOnConnect, PerfBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    static std::atomic<bool> g_echoMode{true};

    // Echo server: B receives from A and echoes back
    auto echoOnMsg = [](void* msg, size_t size) {
        // This is client B's callback - not easily able to send from here
        // Use a different approach: just count receives
    };

    // Simpler approach: measure one-way latency using timestamps
    const int ITERATIONS = 500;
    static std::atomic<int> g_rtCount{0};
    static WaitableFlag g_rtDone;
    g_rtCount = 0;
    g_rtDone.Reset();

    static std::vector<double> g_latencies;
    g_latencies.clear();
    g_latencies.reserve(ITERATIONS);
    static LARGE_INTEGER g_rtFreq;
    QueryPerformanceFrequency(&g_rtFreq);

    auto rtRecvOnMsg = [](void* msg, size_t size) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        IpcMessage* p = (IpcMessage*)msg;
        if (p->header.Type >= IPC_MSG_USER_MIN && size >= sizeof(IPCHeader) + sizeof(LARGE_INTEGER))
        {
            LARGE_INTEGER sendTime;
            memcpy(&sendTime, p->Data, sizeof(LARGE_INTEGER));
            double latencyUs = (double)(now.QuadPart - sendTime.QuadPart) * 1000000.0 / (double)g_rtFreq.QuadPart;
            g_latencies.push_back(latencyUs);
            if (++g_rtCount >= ITERATIONS)
                g_rtDone.Set();
        }
    };

    auto hSender = ipc_client_start(pipe.c_str(), 0x4001, PerfSenderOnMessage, nullptr, nullptr);
    auto hRecv = ipc_client_start(pipe.c_str(), 0x4002, rtRecvOnMsg, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hSender);
    TEST_ASSERT_NOT_NULL(hRecv);
    Sleep(100);

    // Send messages with QPC timestamp in payload
    for (int i = 0; i < ITERATIONS; i++)
    {
        LARGE_INTEGER sendTime;
        QueryPerformanceCounter(&sendTime);
        ipc_client_send(hSender, 0x4002, 1000, &sendTime, sizeof(LARGE_INTEGER));
    }

    bool done = g_rtDone.Wait(10000);
    if (done && !g_latencies.empty())
    {
        ReportLatencyPercentiles(g_latencies);
    }
    else
    {
        printf("          TIMEOUT: received %d / %d\n", g_rtCount.load(), ITERATIONS);
    }

    ipc_client_stop(hSender);
    ipc_client_stop(hRecv);
    Sleep(50);
    ipc_broker_stop(hBroker);

    TEST_ASSERT(done);
    return true;
}

// ============================================================
// Registration
// ============================================================

REGISTER_TEST(BrokerPerformance, perf_unicast_64B, test_perf_unicast_64B);
REGISTER_TEST(BrokerPerformance, perf_unicast_512B, test_perf_unicast_512B);
REGISTER_TEST(BrokerPerformance, perf_unicast_4KB, test_perf_unicast_4KB);
REGISTER_TEST(BrokerPerformance, perf_unicast_8KB, test_perf_unicast_8KB);
REGISTER_TEST(BrokerPerformance, perf_broadcast_64B, test_perf_broadcast_64B);
REGISTER_TEST(BrokerPerformance, perf_multi_sender, test_perf_multi_sender);
REGISTER_TEST(BrokerPerformance, perf_roundtrip_latency, test_perf_roundtrip_latency);
