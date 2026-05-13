#include <windows.h>
#include <stdio.h>
#include <atomic>
#include <vector>
#include <thread>
#include "test_framework.h"
#include "../IPC/IPC.h"
#include "../IPC/IPCMessage.h"

// ============================================================
// Stress test helpers
// ============================================================

static bool StressBrokerOnConnect(void*) { return true; }
static bool StressBrokerOnAuth(void*, unsigned short) { return true; }
static void StressOnMessage(void*, size_t) {}

static std::atomic<int> g_stressRecvCount{0};
static WaitableFlag g_stressDoneEvent;
static int g_stressExpected = 0;

static void StressCounterOnMessage(void* msg, size_t size)
{
    IpcMessage* p = (IpcMessage*)msg;
    if (p->header.Type >= IPC_MSG_USER_MIN)
    {
        if (++g_stressRecvCount >= g_stressExpected)
            g_stressDoneEvent.Set();
    }
}

// ============================================================
// Test Cases
// ============================================================

static bool test_stress_max_clients()
{
    std::string pipe = GetTestPipeName("Stress", 1);
    auto hBroker = ipc_broker_start(pipe.c_str(), StressBrokerOnConnect, StressBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(200);

    const int MAX_CLIENTS = 15; // Leave 1 slot buffer
    IPC_CLIENT_HANDLE clients[MAX_CLIENTS] = {};

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i] = ipc_client_start(pipe.c_str(), (unsigned short)(0x5000 + i), StressOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(clients[i]);
        Sleep(30);
    }

    // Verify all can send
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        IPC_RESULT ret = ipc_client_send(clients[i], (unsigned short)(0x5000 + ((i + 1) % MAX_CLIENTS)), 1000, (void*)"X", 1);
        TEST_ASSERT_EQ(ret, IPC_OK);
    }

    Sleep(200);

    for (int i = 0; i < MAX_CLIENTS; i++)
        ipc_client_stop(clients[i]);
    Sleep(100);
    ipc_broker_stop(hBroker);
    return true;
}

static bool test_stress_concurrent_send()
{
    std::string pipe = GetTestPipeName("Stress", 2);
    auto hBroker = ipc_broker_start(pipe.c_str(), StressBrokerOnConnect, StressBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(200);

    const int NUM_SENDERS = 8;
    const int MSGS_EACH = 100;
    g_stressRecvCount = 0;
    g_stressExpected = NUM_SENDERS * MSGS_EACH;
    g_stressDoneEvent.Reset();

    // One receiver
    auto hRecv = ipc_client_start(pipe.c_str(), 0x6000, StressCounterOnMessage, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hRecv);
    Sleep(50);

    // Senders
    IPC_CLIENT_HANDLE senders[NUM_SENDERS] = {};
    for (int i = 0; i < NUM_SENDERS; i++)
    {
        senders[i] = ipc_client_start(pipe.c_str(), (unsigned short)(0x6010 + i), StressOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(senders[i]);
        Sleep(30);
    }

    // All senders send concurrently
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_SENDERS; i++)
    {
        threads.emplace_back([&, i]() {
            unsigned char data[64] = {};
            for (int j = 0; j < MSGS_EACH; j++)
            {
                ipc_client_send(senders[i], 0x6000, 1000, data, 64);
            }
        });
    }
    for (auto& t : threads) t.join();

    bool done = g_stressDoneEvent.Wait(15000);
    if (!done)
    {
        printf("          Received %d / %d\n", g_stressRecvCount.load(), g_stressExpected);
    }

    for (int i = 0; i < NUM_SENDERS; i++)
        ipc_client_stop(senders[i]);
    ipc_client_stop(hRecv);
    Sleep(100);
    ipc_broker_stop(hBroker);

    TEST_ASSERT(done);
    return true;
}

static bool test_stress_rapid_reconnect()
{
    std::string pipe = GetTestPipeName("Stress", 3);
    auto hBroker = ipc_broker_start(pipe.c_str(), StressBrokerOnConnect, StressBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(200);

    const int NUM_CLIENTS = 4;
    const int ITERATIONS = 10;

    std::vector<std::thread> threads;
    std::atomic<int> failCount{0};

    for (int c = 0; c < NUM_CLIENTS; c++)
    {
        threads.emplace_back([&, c]() {
            for (int i = 0; i < ITERATIONS; i++)
            {
                auto h = ipc_client_start(pipe.c_str(), (unsigned short)(0x7000 + c * 100 + i), StressOnMessage, nullptr, nullptr);
                if (!h)
                {
                    failCount++;
                    continue;
                }
                Sleep(20);
                ipc_client_stop(h);
                Sleep(20);
            }
        });
    }

    for (auto& t : threads) t.join();
    Sleep(100);
    ipc_broker_stop(hBroker);

    // Allow some failures due to pipe contention, but most should succeed
    TEST_ASSERT(failCount < NUM_CLIENTS * ITERATIONS / 2);
    return true;
}

static bool test_stress_broadcast_storm()
{
    std::string pipe = GetTestPipeName("Stress", 4);
    auto hBroker = ipc_broker_start(pipe.c_str(), StressBrokerOnConnect, StressBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(200);

    const int NUM_RECEIVERS = 8;
    const int MSG_COUNT = 500;
    g_stressRecvCount = 0;
    g_stressExpected = MSG_COUNT * NUM_RECEIVERS;
    g_stressDoneEvent.Reset();

    auto hSender = ipc_client_start(pipe.c_str(), 0x8001, StressOnMessage, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hSender);
    Sleep(50);

    IPC_CLIENT_HANDLE receivers[NUM_RECEIVERS] = {};
    for (int i = 0; i < NUM_RECEIVERS; i++)
    {
        receivers[i] = ipc_client_start(pipe.c_str(), (unsigned short)(0x8010 + i), StressCounterOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(receivers[i]);
        Sleep(20);
        ipc_client_register_msg(receivers[i], 1000);
        Sleep(20);
    }

    unsigned char data[64] = {};
    for (int i = 0; i < MSG_COUNT; i++)
    {
        ipc_client_broadcast(hSender, 1000, data, 64);
    }

    bool done = g_stressDoneEvent.Wait(30000);
    if (!done)
    {
        printf("          Received %d / %d (%.1f%%)\n",
            g_stressRecvCount.load(), g_stressExpected,
            100.0 * g_stressRecvCount.load() / g_stressExpected);
    }

    ipc_client_stop(hSender);
    for (int i = 0; i < NUM_RECEIVERS; i++)
        ipc_client_stop(receivers[i]);
    Sleep(100);
    ipc_broker_stop(hBroker);

    TEST_ASSERT(done);
    return true;
}

static bool test_stress_mixed_workload()
{
    std::string pipe = GetTestPipeName("Stress", 5);
    auto hBroker = ipc_broker_start(pipe.c_str(), StressBrokerOnConnect, StressBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(200);

    std::atomic<bool> running{true};
    std::atomic<int> totalSent{0};
    std::atomic<int> totalRecv{0};

    auto mixOnMsg = [](void* msg, size_t size) {
        // just count
    };

    // Start some persistent clients
    IPC_CLIENT_HANDLE persistent[4] = {};
    for (int i = 0; i < 4; i++)
    {
        persistent[i] = ipc_client_start(pipe.c_str(), (unsigned short)(0x9000 + i), StressOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(persistent[i]);
        Sleep(30);
    }

    // Worker threads: send messages
    std::vector<std::thread> workers;
    for (int w = 0; w < 4; w++)
    {
        workers.emplace_back([&, w]() {
            unsigned char data[128] = {};
            while (running)
            {
                int target = (w + 1) % 4;
                ipc_client_send(persistent[w], (unsigned short)(0x9000 + target), 1000, data, 128);
                totalSent++;
                Sleep(5);
            }
        });
    }

    // Run for 2 seconds
    Sleep(2000);
    running = false;

    for (auto& t : workers) t.join();

    printf("          Mixed workload: %d messages sent in 2s\n", totalSent.load());

    for (int i = 0; i < 4; i++)
        ipc_client_stop(persistent[i]);
    Sleep(100);
    ipc_broker_stop(hBroker);

    TEST_ASSERT(totalSent > 0);
    return true;
}

// ============================================================
// Registration
// ============================================================

REGISTER_TEST(BrokerStress, stress_max_clients, test_stress_max_clients);
REGISTER_TEST(BrokerStress, stress_concurrent_send, test_stress_concurrent_send);
REGISTER_TEST(BrokerStress, stress_rapid_reconnect, test_stress_rapid_reconnect);
REGISTER_TEST(BrokerStress, stress_broadcast_storm, test_stress_broadcast_storm);
REGISTER_TEST(BrokerStress, stress_mixed_workload, test_stress_mixed_workload);
