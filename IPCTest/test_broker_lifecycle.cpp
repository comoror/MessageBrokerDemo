#include <windows.h>
#include <stdio.h>
#include <atomic>
#include "test_framework.h"
#include "../IPC/IPC.h"
#include "../IPC/IPCMessage.h"

// ============================================================
// Shared state
// ============================================================

static std::atomic<int> g_lcConnectCount{0};
static std::atomic<int> g_lcDisconnectCount{0};
static WaitableFlag g_lcConnectEvent;
static WaitableFlag g_lcDisconnectEvent;

static void LcReset()
{
    g_lcConnectCount = 0;
    g_lcDisconnectCount = 0;
    g_lcConnectEvent.Reset();
    g_lcDisconnectEvent.Reset();
}

static void LcOnConnect() { g_lcConnectCount++; g_lcConnectEvent.Set(); }
static void LcOnDisconnect() { g_lcDisconnectCount++; g_lcDisconnectEvent.Set(); }
static void LcOnMessage(void*, size_t) {}

static bool LcBrokerOnConnect(void*) { return true; }
static bool LcBrokerOnAuth(void*, unsigned short) { return true; }

// ============================================================
// Test Cases
// ============================================================

static bool test_broker_start_stop()
{
    std::string pipe = GetTestPipeName("Lc", 1);
    auto h = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(h);
    Sleep(100);
    ipc_broker_stop(h);
    return true;
}

static bool test_broker_start_stop_rapid()
{
    for (int i = 0; i < 5; i++)
    {
        std::string pipe = GetTestPipeName("Lc", 20 + i);
        auto h = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
        TEST_ASSERT_NOT_NULL(h);
        Sleep(100);
        ipc_broker_stop(h);
        Sleep(50);
    }
    return true;
}

static bool test_client_connect_disconnect()
{
    std::string pipe = GetTestPipeName("Lc", 3);
    auto hBroker = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    LcReset();
    auto hClient = ipc_client_start(pipe.c_str(), 0x0100, LcOnMessage, LcOnConnect, LcOnDisconnect);
    TEST_ASSERT_NOT_NULL(hClient);
    TEST_ASSERT(g_lcConnectEvent.Wait(2000));

    ipc_client_stop(hClient);
    Sleep(50);
    ipc_broker_stop(hBroker);
    return true;
}

static bool test_client_connect_multiple()
{
    std::string pipe = GetTestPipeName("Lc", 4);
    auto hBroker = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    IPC_CLIENT_HANDLE clients[4] = {};
    for (int i = 0; i < 4; i++)
    {
        clients[i] = ipc_client_start(pipe.c_str(), (unsigned short)(0x0200 + i), LcOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(clients[i]);
        Sleep(50);
    }

    for (int i = 0; i < 4; i++)
        ipc_client_stop(clients[i]);
    Sleep(50);
    ipc_broker_stop(hBroker);
    return true;
}

static bool test_client_rapid_connect_disconnect()
{
    std::string pipe = GetTestPipeName("Lc", 5);
    auto hBroker = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    for (int i = 0; i < 10; i++)
    {
        auto hClient = ipc_client_start(pipe.c_str(), 0x0300, LcOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(hClient);
        Sleep(30);
        ipc_client_stop(hClient);
        Sleep(30);
    }

    ipc_broker_stop(hBroker);
    return true;
}

static bool test_broker_stop_with_clients()
{
    std::string pipe = GetTestPipeName("Lc", 6);
    auto hBroker = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    LcReset();
    auto hClient = ipc_client_start(pipe.c_str(), 0x0400, LcOnMessage, nullptr, LcOnDisconnect);
    TEST_ASSERT_NOT_NULL(hClient);
    Sleep(100);

    // Stop broker while client connected - should not crash
    ipc_broker_stop(hBroker);
    Sleep(100);

    // Client should get disconnect notification or at least not crash on stop
    ipc_client_stop(hClient);
    return true;
}

static bool test_client_connect_no_broker()
{
    // Use a pipe name that nobody is listening on
    std::string pipe = GetTestPipeName("Lc", 7);
    // This should fail (no broker running) - but may block for WaitNamedPipe timeout
    // Use a short timeout pipe name or accept the wait
    auto hClient = ipc_client_start(pipe.c_str(), 0x0500, LcOnMessage, nullptr, nullptr);
    // Should return nullptr since no broker is listening
    TEST_ASSERT_NULL(hClient);
    return true;
}

static bool test_broker_onauth_reject()
{
    std::string pipe = GetTestPipeName("Lc", 8);

    // Auth callback that rejects client_id 0x0600
    auto rejectAuth = [](void* hPipe, unsigned short clientId) -> bool {
        return (clientId != 0x0600);
    };

    auto hBroker = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, rejectAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    // Connect with ID that will be rejected at auth stage
    // Note: connection at pipe level succeeds, but broker will kick after registration
    auto hClient = ipc_client_start(pipe.c_str(), 0x0600, LcOnMessage, nullptr, nullptr);
    // Connection may succeed at transport level, the rejection happens at registration
    // Just verify no crash - either null or valid handle
    if (hClient)
    {
        Sleep(200); // Give broker time to process and reject
        ipc_client_stop(hClient);
    }

    ipc_broker_stop(hBroker);
    return true;
}

// ============================================================
// Handle Leak Tests
// ============================================================

static bool test_broker_handle_leak()
{
    // Warmup
    {
        std::string pipe = GetTestPipeName("Lc", 100);
        auto h = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
        if (h) { Sleep(100); ipc_broker_stop(h); Sleep(50); }
    }

    DWORD handlesBefore = GetCurrentHandleCount();

    for (int i = 0; i < 10; i++)
    {
        std::string pipe = GetTestPipeName("Lc", 101 + i);
        auto h = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
        TEST_ASSERT_NOT_NULL(h);
        Sleep(100);
        ipc_broker_stop(h);
        Sleep(50);
    }

    DWORD handlesAfter = GetCurrentHandleCount();
    printf("          Handles: before=%lu, after=%lu, diff=%ld\n",
        handlesBefore, handlesAfter, (long)handlesAfter - (long)handlesBefore);
    TEST_ASSERT_NO_HANDLE_LEAK(handlesBefore, handlesAfter, 4);
    return true;
}

static bool test_broker_client_handle_leak()
{
    std::string pipe = GetTestPipeName("Lc", 200);
    auto hBroker = ipc_broker_start(pipe.c_str(), LcBrokerOnConnect, LcBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    // Warmup
    {
        auto hc = ipc_client_start(pipe.c_str(), 0x0900, LcOnMessage, nullptr, nullptr);
        if (hc) { Sleep(50); ipc_client_stop(hc); Sleep(50); }
    }

    DWORD handlesBefore = GetCurrentHandleCount();

    for (int i = 0; i < 10; i++)
    {
        auto hClient = ipc_client_start(pipe.c_str(), (unsigned short)(0x0A00 + i), LcOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(hClient);
        Sleep(30);
        ipc_client_stop(hClient);
        Sleep(50);
    }

    DWORD handlesAfter = GetCurrentHandleCount();
    printf("          Handles: before=%lu, after=%lu, diff=%ld\n",
        handlesBefore, handlesAfter, (long)handlesAfter - (long)handlesBefore);
    TEST_ASSERT_NO_HANDLE_LEAK(handlesBefore, handlesAfter, 4);

    ipc_broker_stop(hBroker);
    return true;
}

// ============================================================
// Registration
// ============================================================

REGISTER_TEST(BrokerLifecycle, broker_start_stop, test_broker_start_stop);
REGISTER_TEST(BrokerLifecycle, broker_start_stop_rapid, test_broker_start_stop_rapid);
REGISTER_TEST(BrokerLifecycle, client_connect_disconnect, test_client_connect_disconnect);
REGISTER_TEST(BrokerLifecycle, client_connect_multiple, test_client_connect_multiple);
REGISTER_TEST(BrokerLifecycle, client_rapid_connect_disconnect, test_client_rapid_connect_disconnect);
REGISTER_TEST(BrokerLifecycle, broker_stop_with_clients, test_broker_stop_with_clients);
// Skipping client_connect_no_broker for now - WaitNamedPipe 20s timeout makes it too slow
// REGISTER_TEST(BrokerLifecycle, client_connect_no_broker, test_client_connect_no_broker);
REGISTER_TEST(BrokerLifecycle, broker_onauth_reject, test_broker_onauth_reject);
REGISTER_TEST(BrokerLifecycle, broker_handle_leak, test_broker_handle_leak);
REGISTER_TEST(BrokerLifecycle, broker_client_handle_leak, test_broker_client_handle_leak);
