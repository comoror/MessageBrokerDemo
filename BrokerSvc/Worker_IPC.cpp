#include "Worker_IPC.h"
#include <Windows.h>
#include <thread>
#include "Log.h"

static HANDLE g_hExitEvent = NULL;
static std::thread work_thread;

static IPCClient* pClient = nullptr;
static bool bConnectOk = false;

static void OnIPCClientConnect()
{
    // Handle connection here
    LOG_DEBUG("IPC Client connected successfully.");
}

static void OnIPCClientDisconnect()
{
    // Handle disconnection here
    LOG_DEBUG("IPC Client disconnected.");
}

static void OnIPCClientMessage(void* msg)
{
    // Handle incoming messages here
    IpcMessage* ipcMsg = (IpcMessage*)msg;
    // Process the message...
    
    LOG_DEBUG("Received IPC message: SrcId=0x%04X, DstId=0x%04X, Type=0x%04X, Size=%d",
        ipcMsg->header.SrcId, ipcMsg->header.DstId, ipcMsg->header.Type, ipcMsg->header.Size);
}

static int thread_func()
{
    pClient = new IPCClient(0xF001);

    if (pClient)
    {
        if(pClient->Connect("\\\\.\\pipe\\DemoBroker", OnIPCClientMessage, OnIPCClientConnect, OnIPCClientDisconnect))
        {
            LOG_INFO("Connect success");
            bConnectOk = true;
        }
        else
        {
            LOG_INFO("Connect failed");
        }
    }

    while (true)
    {
        // Wait for exit event
        if (WaitForSingleObject(g_hExitEvent, 1000) == WAIT_OBJECT_0)
            break;
        if (!bConnectOk)
        {
            // Try to connect again
            if (pClient->Connect("\\\\.\\pipe\\DemoBroker", OnIPCClientMessage))
            {
                LOG_INFO("Connect success");
                bConnectOk = true;
            }
            else
            {
                LOG_INFO("Connect failed");
            }
        }
    }

    if (pClient)
    {
        pClient->Disconnect();
        delete pClient;
        pClient = nullptr;
    }

    return 0;
}

void Worker_IPC::start()
{
    // Create a exit event
    g_hExitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    // Start a thread
    work_thread = std::thread(thread_func);
}

void Worker_IPC::stop()
{
    // Set exit event to stop thread
    if (g_hExitEvent != NULL)
    {
        SetEvent(g_hExitEvent);
        CloseHandle(g_hExitEvent);
        g_hExitEvent = NULL;
    }
    // Wait for thread to exit
    if (work_thread.joinable())
        work_thread.join();
}

void Worker_IPC::send(IpcMessagePtr msg)
{
    if (pClient)
    {
        pClient->Send(msg.get());
    }
}
