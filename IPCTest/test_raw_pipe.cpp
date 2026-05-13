#include <windows.h>
#include <stdio.h>
#include <atomic>
#include <string.h>
#include "test_framework.h"
#include "../IPC/IPC.h"

// ============================================================
// Shared state for raw pipe server callbacks
// ============================================================

struct RawServerCtx
{
    std::atomic<int> connectCount{0};
    std::atomic<int> disconnectCount{0};
    std::atomic<int> msgCount{0};
    WaitableFlag msgEvent;
    WaitableFlag connectEvent;
    WaitableFlag disconnectEvent;
    unsigned long lastPipeIndex = 0;
    char lastData[256] = {};
    size_t lastDataSize = 0;
    IPC_PIPE_SERVER_HANDLE hServer = nullptr;
};

static void RawSrvOnMessage(void* ctx, unsigned long pipeIndex, void* data, size_t size)
{
    RawServerCtx* c = (RawServerCtx*)ctx;
    c->lastPipeIndex = pipeIndex;
    size_t copySize = (size < sizeof(c->lastData)) ? size : sizeof(c->lastData) - 1;
    memcpy(c->lastData, data, copySize);
    c->lastDataSize = size;
    c->msgCount++;
    c->msgEvent.Set();
}

static void RawSrvOnConnect(void* ctx, unsigned long pipeIndex)
{
    RawServerCtx* c = (RawServerCtx*)ctx;
    c->lastPipeIndex = pipeIndex;
    c->connectCount++;
    c->connectEvent.Set();
}

static void RawSrvOnDisconnect(void* ctx, unsigned long pipeIndex)
{
    RawServerCtx* c = (RawServerCtx*)ctx;
    c->disconnectCount++;
    c->disconnectEvent.Set();
}

// ============================================================
// Shared state for raw pipe client callbacks
// ============================================================

static std::atomic<int> g_rawClientMsgCount{0};
static WaitableFlag g_rawClientMsgEvent;
static char g_rawClientLastData[256] = {};
static size_t g_rawClientLastSize = 0;
static std::atomic<int> g_rawClientConnectCount{0};
static WaitableFlag g_rawClientConnectEvent;
static std::atomic<int> g_rawClientDisconnectCount{0};
static WaitableFlag g_rawClientDisconnectEvent;

static void RawClientReset()
{
    g_rawClientMsgCount = 0;
    g_rawClientMsgEvent.Reset();
    memset(g_rawClientLastData, 0, sizeof(g_rawClientLastData));
    g_rawClientLastSize = 0;
    g_rawClientConnectCount = 0;
    g_rawClientConnectEvent.Reset();
    g_rawClientDisconnectCount = 0;
    g_rawClientDisconnectEvent.Reset();
}

static void RawCliOnMessage(void* data, size_t size)
{
    size_t copySize = (size < sizeof(g_rawClientLastData)) ? size : sizeof(g_rawClientLastData) - 1;
    memcpy(g_rawClientLastData, data, copySize);
    g_rawClientLastSize = size;
    g_rawClientMsgCount++;
    g_rawClientMsgEvent.Set();
}

static void RawCliOnConnect()
{
    g_rawClientConnectCount++;
    g_rawClientConnectEvent.Set();
}

static void RawCliOnDisconnect()
{
    g_rawClientDisconnectCount++;
    g_rawClientDisconnectEvent.Set();
}

// ============================================================
// Test Cases
// ============================================================

static bool test_raw_server_start_stop()
{
    std::string pipeName = GetTestPipeName("Raw", 1);
    RawServerCtx ctx;
    auto h = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, nullptr, &ctx);
    TEST_ASSERT_NOT_NULL(h);
    Sleep(50);
    ipc_pipe_server_stop(h);
    return true;
}

static bool test_raw_server_null_params()
{
    auto h1 = ipc_pipe_server_start(nullptr, RawSrvOnMessage, nullptr, nullptr, nullptr);
    TEST_ASSERT_NULL(h1);

    std::string pipeName = GetTestPipeName("Raw", 2);
    auto h2 = ipc_pipe_server_start(pipeName.c_str(), nullptr, nullptr, nullptr, nullptr);
    TEST_ASSERT_NULL(h2);
    return true;
}

static bool test_raw_client_connect_disconnect()
{
    std::string pipeName = GetTestPipeName("Raw", 3);
    RawServerCtx ctx;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, RawSrvOnDisconnect, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    RawClientReset();
    auto hClient = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, RawCliOnConnect, RawCliOnDisconnect);
    TEST_ASSERT_NOT_NULL(hClient);

    TEST_ASSERT(ctx.connectEvent.Wait(2000));
    TEST_ASSERT(g_rawClientConnectEvent.Wait(2000));
    TEST_ASSERT_EQ(ctx.connectCount.load(), 1);

    ipc_pipe_client_disconnect(hClient);
    Sleep(100);
    ipc_pipe_server_stop(hServer);
    return true;
}

static bool test_raw_client_send_receive()
{
    std::string pipeName = GetTestPipeName("Raw", 4);
    RawServerCtx ctx;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, nullptr, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    RawClientReset();
    auto hClient = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, RawCliOnConnect, nullptr);
    TEST_ASSERT_NOT_NULL(hClient);
    TEST_ASSERT(ctx.connectEvent.Wait(2000));

    const char* testMsg = "Hello Raw Pipe";
    IPC_RESULT ret = ipc_pipe_client_send(hClient, (void*)testMsg, strlen(testMsg));
    TEST_ASSERT_EQ(ret, IPC_OK);

    TEST_ASSERT(ctx.msgEvent.Wait(2000));
    TEST_ASSERT_EQ(ctx.lastDataSize, strlen(testMsg));
    TEST_ASSERT(memcmp(ctx.lastData, testMsg, strlen(testMsg)) == 0);

    ipc_pipe_client_disconnect(hClient);
    Sleep(50);
    ipc_pipe_server_stop(hServer);
    return true;
}

static bool test_raw_server_send_to_client()
{
    std::string pipeName = GetTestPipeName("Raw", 5);
    RawServerCtx ctx;
    ctx.hServer = nullptr;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, nullptr, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    RawClientReset();
    auto hClient = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, RawCliOnConnect, nullptr);
    TEST_ASSERT_NOT_NULL(hClient);
    TEST_ASSERT(ctx.connectEvent.Wait(2000));

    const char* reply = "ServerReply";
    IPC_RESULT ret = ipc_pipe_server_send(hServer, ctx.lastPipeIndex, (void*)reply, strlen(reply));
    TEST_ASSERT_EQ(ret, IPC_OK);

    TEST_ASSERT(g_rawClientMsgEvent.Wait(2000));
    TEST_ASSERT_EQ(g_rawClientLastSize, strlen(reply));
    TEST_ASSERT(memcmp(g_rawClientLastData, reply, strlen(reply)) == 0);

    ipc_pipe_client_disconnect(hClient);
    Sleep(50);
    ipc_pipe_server_stop(hServer);
    return true;
}

static bool test_raw_server_broadcast()
{
    std::string pipeName = GetTestPipeName("Raw", 6);
    RawServerCtx ctx;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, nullptr, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    // Connect 3 clients - use simple shared counters
    static std::atomic<int> broadcastRecvCount{0};
    static WaitableFlag broadcastDoneEvent;
    broadcastRecvCount = 0;
    broadcastDoneEvent.Reset();

    auto bcOnMsg = [](void* data, size_t size) {
        if (++broadcastRecvCount >= 3)
            broadcastDoneEvent.Set();
    };

    auto hC1 = ipc_pipe_client_connect(pipeName.c_str(), bcOnMsg, nullptr, nullptr);
    Sleep(100);
    auto hC2 = ipc_pipe_client_connect(pipeName.c_str(), bcOnMsg, nullptr, nullptr);
    Sleep(100);
    auto hC3 = ipc_pipe_client_connect(pipeName.c_str(), bcOnMsg, nullptr, nullptr);
    Sleep(100);

    TEST_ASSERT_NOT_NULL(hC1);
    TEST_ASSERT_NOT_NULL(hC2);
    TEST_ASSERT_NOT_NULL(hC3);

    const char* bcMsg = "BroadcastMsg";
    ipc_pipe_server_broadcast(hServer, (void*)bcMsg, strlen(bcMsg));

    TEST_ASSERT(broadcastDoneEvent.Wait(3000));
    TEST_ASSERT(broadcastRecvCount >= 3);

    ipc_pipe_client_disconnect(hC1);
    ipc_pipe_client_disconnect(hC2);
    ipc_pipe_client_disconnect(hC3);
    Sleep(50);
    ipc_pipe_server_stop(hServer);
    return true;
}

static bool test_raw_server_disconnect_client()
{
    std::string pipeName = GetTestPipeName("Raw", 7);
    RawServerCtx ctx;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, RawSrvOnDisconnect, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    RawClientReset();
    auto hClient = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, nullptr, RawCliOnDisconnect);
    TEST_ASSERT_NOT_NULL(hClient);
    TEST_ASSERT(ctx.connectEvent.Wait(2000));

    ipc_pipe_server_disconnect_client(hServer, ctx.lastPipeIndex);
    TEST_ASSERT(g_rawClientDisconnectEvent.Wait(2000));

    ipc_pipe_client_disconnect(hClient);
    Sleep(50);
    ipc_pipe_server_stop(hServer);
    return true;
}

static bool test_raw_multiple_clients()
{
    std::string pipeName = GetTestPipeName("Raw", 8);
    RawServerCtx ctx;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, nullptr, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    IPC_PIPE_CLIENT_HANDLE clients[4] = {};
    for (int i = 0; i < 4; i++)
    {
        clients[i] = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(clients[i]);
        Sleep(100);
    }

    TEST_ASSERT(ctx.connectCount >= 4);

    // Each client sends a unique message
    for (int i = 0; i < 4; i++)
    {
        ctx.msgEvent.Reset();
        char msg[32];
        sprintf_s(msg, "client_%d", i);
        ipc_pipe_client_send(clients[i], msg, strlen(msg));
        TEST_ASSERT(ctx.msgEvent.Wait(2000));
    }

    TEST_ASSERT(ctx.msgCount >= 4);

    for (int i = 0; i < 4; i++)
        ipc_pipe_client_disconnect(clients[i]);
    Sleep(50);
    ipc_pipe_server_stop(hServer);
    return true;
}

static bool test_raw_bidirectional()
{
    std::string pipeName = GetTestPipeName("Raw", 9);
    RawServerCtx ctx;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, nullptr, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    RawClientReset();
    auto hClient = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, RawCliOnConnect, nullptr);
    TEST_ASSERT_NOT_NULL(hClient);
    TEST_ASSERT(ctx.connectEvent.Wait(2000));

    // Client -> Server
    const char* req = "REQUEST";
    ipc_pipe_client_send(hClient, (void*)req, strlen(req));
    TEST_ASSERT(ctx.msgEvent.Wait(2000));
    TEST_ASSERT(memcmp(ctx.lastData, req, strlen(req)) == 0);

    // Server -> Client
    const char* resp = "RESPONSE";
    ipc_pipe_server_send(hServer, ctx.lastPipeIndex, (void*)resp, strlen(resp));
    TEST_ASSERT(g_rawClientMsgEvent.Wait(2000));
    TEST_ASSERT(memcmp(g_rawClientLastData, resp, strlen(resp)) == 0);

    ipc_pipe_client_disconnect(hClient);
    Sleep(50);
    ipc_pipe_server_stop(hServer);
    return true;
}

// ============================================================
// Handle Leak Tests
// ============================================================

static bool test_raw_server_handle_leak()
{
    // Warmup: one start/stop cycle to stabilize thread pool etc.
    {
        std::string pipeName = GetTestPipeName("Raw", 100);
        RawServerCtx ctx;
        auto h = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, nullptr, nullptr, &ctx);
        if (h) { Sleep(50); ipc_pipe_server_stop(h); Sleep(50); }
    }

    DWORD handlesBefore = GetCurrentHandleCount();

    // Repeat start/stop 10 times
    for (int i = 0; i < 10; i++)
    {
        std::string pipeName = GetTestPipeName("Raw", 101 + i);
        RawServerCtx ctx;
        auto h = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, nullptr, nullptr, &ctx);
        TEST_ASSERT_NOT_NULL(h);
        Sleep(50);
        ipc_pipe_server_stop(h);
        Sleep(50);
    }

    DWORD handlesAfter = GetCurrentHandleCount();
    printf("          Handles: before=%lu, after=%lu, diff=%ld\n",
        handlesBefore, handlesAfter, (long)handlesAfter - (long)handlesBefore);
    TEST_ASSERT_NO_HANDLE_LEAK(handlesBefore, handlesAfter, 4);
    return true;
}

static bool test_raw_client_handle_leak()
{
    std::string pipeName = GetTestPipeName("Raw", 200);
    RawServerCtx ctx;
    auto hServer = ipc_pipe_server_start(pipeName.c_str(), RawSrvOnMessage, RawSrvOnConnect, RawSrvOnDisconnect, &ctx);
    TEST_ASSERT_NOT_NULL(hServer);
    Sleep(100);

    // Warmup
    {
        RawClientReset();
        auto hc = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, nullptr, nullptr);
        if (hc) { Sleep(50); ipc_pipe_client_disconnect(hc); Sleep(50); }
    }

    DWORD handlesBefore = GetCurrentHandleCount();

    // Connect/disconnect 10 times
    for (int i = 0; i < 10; i++)
    {
        RawClientReset();
        auto hClient = ipc_pipe_client_connect(pipeName.c_str(), RawCliOnMessage, nullptr, nullptr);
        TEST_ASSERT_NOT_NULL(hClient);
        Sleep(30);
        ipc_pipe_client_disconnect(hClient);
        Sleep(50);
    }

    DWORD handlesAfter = GetCurrentHandleCount();
    printf("          Handles: before=%lu, after=%lu, diff=%ld\n",
        handlesBefore, handlesAfter, (long)handlesAfter - (long)handlesBefore);
    TEST_ASSERT_NO_HANDLE_LEAK(handlesBefore, handlesAfter, 4);

    ipc_pipe_server_stop(hServer);
    return true;
}

// ============================================================
// Registration
// ============================================================

REGISTER_TEST(RawPipe, raw_server_start_stop, test_raw_server_start_stop);
REGISTER_TEST(RawPipe, raw_server_null_params, test_raw_server_null_params);
REGISTER_TEST(RawPipe, raw_client_connect_disconnect, test_raw_client_connect_disconnect);
REGISTER_TEST(RawPipe, raw_client_send_receive, test_raw_client_send_receive);
REGISTER_TEST(RawPipe, raw_server_send_to_client, test_raw_server_send_to_client);
REGISTER_TEST(RawPipe, raw_server_broadcast, test_raw_server_broadcast);
REGISTER_TEST(RawPipe, raw_server_disconnect_client, test_raw_server_disconnect_client);
REGISTER_TEST(RawPipe, raw_multiple_clients, test_raw_multiple_clients);
REGISTER_TEST(RawPipe, raw_bidirectional, test_raw_bidirectional);
REGISTER_TEST(RawPipe, raw_server_handle_leak, test_raw_server_handle_leak);
REGISTER_TEST(RawPipe, raw_client_handle_leak, test_raw_client_handle_leak);
