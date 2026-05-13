#include <windows.h>
#include <stdio.h>
#include <atomic>
#include <string.h>
#include "test_framework.h"
#include "../IPC/IPC.h"
#include "../IPC/IPCMessage.h"

// ============================================================
// Shared state for functional tests
// ============================================================

static std::atomic<int> g_fnMsgCount{0};
static WaitableFlag g_fnMsgEvent;
static IpcMessage g_fnLastMsg = IpcMessage(0, 0, 0, nullptr, 0);
static std::atomic<int> g_fnMsgCountB{0};
static WaitableFlag g_fnMsgEventB;
static IpcMessage g_fnLastMsgB = IpcMessage(0, 0, 0, nullptr, 0);

static void FnReset()
{
    g_fnMsgCount = 0;
    g_fnMsgEvent.Reset();
    g_fnMsgCountB = 0;
    g_fnMsgEventB.Reset();
}

static void FnOnMessageA(void* msg, size_t size)
{
    if (size >= sizeof(IPCHeader))
        memcpy(&g_fnLastMsg, msg, (size < sizeof(IpcMessage)) ? size : sizeof(IpcMessage));
    g_fnMsgCount++;
    g_fnMsgEvent.Set();
}

static void FnOnMessageB(void* msg, size_t size)
{
    if (size >= sizeof(IPCHeader))
        memcpy(&g_fnLastMsgB, msg, (size < sizeof(IpcMessage)) ? size : sizeof(IpcMessage));
    g_fnMsgCountB++;
    g_fnMsgEventB.Set();
}

static bool FnBrokerOnConnect(void*) { return true; }
static bool FnBrokerOnAuth(void*, unsigned short) { return true; }

// Helper: start broker + 2 clients
struct BrokerFixture
{
    IPC_BROKER_HANDLE hBroker = nullptr;
    IPC_CLIENT_HANDLE hClientA = nullptr;
    IPC_CLIENT_HANDLE hClientB = nullptr;
    std::string pipeName;

    bool Setup(int testIndex, unsigned short idA = 0xA001, unsigned short idB = 0xB001)
    {
        pipeName = GetTestPipeName("Fn", testIndex);
        hBroker = ipc_broker_start(pipeName.c_str(), FnBrokerOnConnect, FnBrokerOnAuth, nullptr);
        if (!hBroker) return false;
        Sleep(150);

        FnReset();
        hClientA = ipc_client_start(pipeName.c_str(), idA, FnOnMessageA, nullptr, nullptr);
        if (!hClientA) { ipc_broker_stop(hBroker); return false; }
        Sleep(50);

        hClientB = ipc_client_start(pipeName.c_str(), idB, FnOnMessageB, nullptr, nullptr);
        if (!hClientB) { ipc_client_stop(hClientA); ipc_broker_stop(hBroker); return false; }
        Sleep(50);
        return true;
    }

    void Teardown()
    {
        if (hClientA) { ipc_client_stop(hClientA); hClientA = nullptr; }
        if (hClientB) { ipc_client_stop(hClientB); hClientB = nullptr; }
        Sleep(50);
        if (hBroker) { ipc_broker_stop(hBroker); hBroker = nullptr; }
    }
};

// ============================================================
// Test Cases
// ============================================================

static bool test_unicast_delivery()
{
    BrokerFixture f;
    TEST_ASSERT(f.Setup(1));

    IPC_RESULT ret = ipc_client_send(f.hClientA, 0xB001, 1000, (void*)"Hi B", 4);
    TEST_ASSERT_EQ(ret, IPC_OK);

    TEST_ASSERT(g_fnMsgEventB.Wait(2000));
    TEST_ASSERT_EQ(g_fnLastMsgB.header.Type, 1000);
    TEST_ASSERT_EQ(g_fnLastMsgB.header.SrcId, 0xA001);
    TEST_ASSERT(memcmp(g_fnLastMsgB.Data, "Hi B", 4) == 0);

    f.Teardown();
    return true;
}

static bool test_unicast_reverse()
{
    BrokerFixture f;
    TEST_ASSERT(f.Setup(2));

    IPC_RESULT ret = ipc_client_send(f.hClientB, 0xA001, 2000, (void*)"Hi A", 4);
    TEST_ASSERT_EQ(ret, IPC_OK);

    TEST_ASSERT(g_fnMsgEvent.Wait(2000));
    TEST_ASSERT_EQ(g_fnLastMsg.header.Type, 2000);
    TEST_ASSERT_EQ(g_fnLastMsg.header.SrcId, 0xB001);

    f.Teardown();
    return true;
}

static bool test_unicast_dst_not_found()
{
    BrokerFixture f;
    TEST_ASSERT(f.Setup(3));

    // Send to non-existent client
    g_fnMsgEvent.Reset();
    g_fnMsgCount = 0;
    IPC_RESULT ret = ipc_client_send(f.hClientA, 0x9999, 1000, (void*)"X", 1);
    TEST_ASSERT_EQ(ret, IPC_OK);

    // Should receive DST_NOT_FOUND from broker
    TEST_ASSERT(g_fnMsgEvent.Wait(2000));
    TEST_ASSERT_EQ(g_fnLastMsg.header.Type, IPC_MSG_DST_NOT_FOUND);

    f.Teardown();
    return true;
}

static bool test_broadcast_with_registration()
{
    BrokerFixture f;
    TEST_ASSERT(f.Setup(4));

    // B registers for msg_type 1000
    IPC_RESULT ret = ipc_client_register_msg(f.hClientB, 1000);
    TEST_ASSERT_EQ(ret, IPC_OK);
    Sleep(50);

    // A broadcasts type 1000
    g_fnMsgEventB.Reset();
    g_fnMsgCountB = 0;
    ret = ipc_client_broadcast(f.hClientA, 1000, (void*)"BC", 2);
    TEST_ASSERT_EQ(ret, IPC_OK);

    TEST_ASSERT(g_fnMsgEventB.Wait(2000));
    TEST_ASSERT_EQ(g_fnLastMsgB.header.Type, 1000);
    TEST_ASSERT_EQ(g_fnLastMsgB.header.DstId, IPC_BROADCAST);

    f.Teardown();
    return true;
}

static bool test_broadcast_multiple_receivers()
{
    std::string pipe = GetTestPipeName("Fn", 5);
    auto hBroker = ipc_broker_start(pipe.c_str(), FnBrokerOnConnect, FnBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    static std::atomic<int> multiRecvCount{0};
    static WaitableFlag multiDone;
    multiRecvCount = 0;
    multiDone.Reset();

    auto onRecv = [](void* msg, size_t size) {
        IpcMessage* p = (IpcMessage*)msg;
        if (p->header.Type >= IPC_MSG_USER_MIN)
        {
            if (++multiRecvCount >= 2)
                multiDone.Set();
        }
    };

    auto hSender = ipc_client_start(pipe.c_str(), 0xC001, FnOnMessageA, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hSender);
    Sleep(50);

    auto hRecvB = ipc_client_start(pipe.c_str(), 0xC002, onRecv, nullptr, nullptr);
    auto hRecvC = ipc_client_start(pipe.c_str(), 0xC003, onRecv, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hRecvB);
    TEST_ASSERT_NOT_NULL(hRecvC);
    Sleep(50);

    ipc_client_register_msg(hRecvB, 1000);
    ipc_client_register_msg(hRecvC, 1000);
    Sleep(50);

    ipc_client_broadcast(hSender, 1000, (void*)"ALL", 3);
    TEST_ASSERT(multiDone.Wait(3000));
    TEST_ASSERT(multiRecvCount >= 2);

    ipc_client_stop(hSender);
    ipc_client_stop(hRecvB);
    ipc_client_stop(hRecvC);
    Sleep(50);
    ipc_broker_stop(hBroker);
    return true;
}

static bool test_broadcast_no_receivers()
{
    BrokerFixture f;
    TEST_ASSERT(f.Setup(6));

    // No one registered for type 2000 -> broadcast should not crash
    IPC_RESULT ret = ipc_client_broadcast(f.hClientA, 2000, (void*)"NR", 2);
    TEST_ASSERT_EQ(ret, IPC_OK);
    Sleep(200);
    // No message received by B (not registered)
    TEST_ASSERT_EQ(g_fnMsgCountB.load(), 0);

    f.Teardown();
    return true;
}

static bool test_duplicate_client_kick()
{
    std::string pipe = GetTestPipeName("Fn", 8);
    auto hBroker = ipc_broker_start(pipe.c_str(), FnBrokerOnConnect, FnBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    FnReset();
    // First client with ID 0xD001
    auto hFirst = ipc_client_start(pipe.c_str(), 0xD001, FnOnMessageA, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hFirst);
    Sleep(100);

    // Second client with same ID 0xD001 -> should kick first
    auto hSecond = ipc_client_start(pipe.c_str(), 0xD001, FnOnMessageB, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hSecond);

    // First client should receive KICK message
    TEST_ASSERT(g_fnMsgEvent.Wait(3000));
    TEST_ASSERT_EQ(g_fnLastMsg.header.Type, IPC_MSG_KICK);

    ipc_client_stop(hFirst);
    ipc_client_stop(hSecond);
    Sleep(50);
    ipc_broker_stop(hBroker);
    return true;
}

static bool test_payload_integrity()
{
    BrokerFixture f;
    TEST_ASSERT(f.Setup(9));

    // Create 4KB binary payload with pattern
    unsigned char payload[4096];
    for (int i = 0; i < 4096; i++)
        payload[i] = (unsigned char)(i & 0xFF);

    IPC_RESULT ret = ipc_client_send(f.hClientA, 0xB001, 1000, payload, 4096);
    TEST_ASSERT_EQ(ret, IPC_OK);

    TEST_ASSERT(g_fnMsgEventB.Wait(2000));
    unsigned short payloadLen = g_fnLastMsgB.header.Size - sizeof(IPCHeader);
    TEST_ASSERT_EQ(payloadLen, 4096);
    TEST_ASSERT(memcmp(g_fnLastMsgB.Data, payload, 4096) == 0);

    f.Teardown();
    return true;
}

static bool test_max_payload()
{
    BrokerFixture f;
    TEST_ASSERT(f.Setup(10));

    unsigned char payload[IpcMessage::MAX_PAYLOAD_SIZE];
    memset(payload, 0xAB, sizeof(payload));

    IPC_RESULT ret = ipc_client_send(f.hClientA, 0xB001, 1000, payload, IpcMessage::MAX_PAYLOAD_SIZE);
    TEST_ASSERT_EQ(ret, IPC_OK);

    TEST_ASSERT(g_fnMsgEventB.Wait(3000));
    unsigned short payloadLen = g_fnLastMsgB.header.Size - sizeof(IPCHeader);
    TEST_ASSERT_EQ(payloadLen, IpcMessage::MAX_PAYLOAD_SIZE);

    f.Teardown();
    return true;
}

static bool test_register_multiple_types()
{
    std::string pipe = GetTestPipeName("Fn", 11);
    auto hBroker = ipc_broker_start(pipe.c_str(), FnBrokerOnConnect, FnBrokerOnAuth, nullptr);
    TEST_ASSERT_NOT_NULL(hBroker);
    Sleep(150);

    static std::atomic<int> multiTypeCount{0};
    static WaitableFlag multiTypeDone;
    multiTypeCount = 0;
    multiTypeDone.Reset();

    auto onRecv = [](void* msg, size_t size) {
        IpcMessage* p = (IpcMessage*)msg;
        if (p->header.Type >= IPC_MSG_USER_MIN)
        {
            if (++multiTypeCount >= 3)
                multiTypeDone.Set();
        }
    };

    auto hSender = ipc_client_start(pipe.c_str(), 0xE001, FnOnMessageA, nullptr, nullptr);
    auto hRecv = ipc_client_start(pipe.c_str(), 0xE002, onRecv, nullptr, nullptr);
    TEST_ASSERT_NOT_NULL(hSender);
    TEST_ASSERT_NOT_NULL(hRecv);
    Sleep(50);

    ipc_client_register_msg(hRecv, 10);
    ipc_client_register_msg(hRecv, 11);
    ipc_client_register_msg(hRecv, 12);
    Sleep(50);

    ipc_client_broadcast(hSender, 10, (void*)"A", 1);
    ipc_client_broadcast(hSender, 11, (void*)"B", 1);
    ipc_client_broadcast(hSender, 12, (void*)"C", 1);

    TEST_ASSERT(multiTypeDone.Wait(3000));
    TEST_ASSERT(multiTypeCount >= 3);

    ipc_client_stop(hSender);
    ipc_client_stop(hRecv);
    Sleep(50);
    ipc_broker_stop(hBroker);
    return true;
}

// ============================================================
// Registration
// ============================================================

REGISTER_TEST(BrokerFunctional, unicast_delivery, test_unicast_delivery);
REGISTER_TEST(BrokerFunctional, unicast_reverse, test_unicast_reverse);
REGISTER_TEST(BrokerFunctional, unicast_dst_not_found, test_unicast_dst_not_found);
REGISTER_TEST(BrokerFunctional, broadcast_with_registration, test_broadcast_with_registration);
REGISTER_TEST(BrokerFunctional, broadcast_multiple_receivers, test_broadcast_multiple_receivers);
REGISTER_TEST(BrokerFunctional, broadcast_no_receivers, test_broadcast_no_receivers);
REGISTER_TEST(BrokerFunctional, duplicate_client_kick, test_duplicate_client_kick);
REGISTER_TEST(BrokerFunctional, payload_integrity, test_payload_integrity);
REGISTER_TEST(BrokerFunctional, max_payload, test_max_payload);
REGISTER_TEST(BrokerFunctional, register_multiple_types, test_register_multiple_types);
