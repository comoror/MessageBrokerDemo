#include "Worker_IPCBroker.h"
#include "Log.h"
#include <thread>
#include "..\IPC\IPC.h"
#include "..\IPC\IPCMessage.h"

#define IPC_BROKER_PIPE "\\\\.\\pipe\\PCManager_IPCBroker"

static void* g_ipc_broker = nullptr;

int Worker_IPCBroker::start()
{
    if (g_ipc_broker != nullptr) {
        LOG_WARN("IPC Broker already started.");
        return -1;
    }
    g_ipc_broker = ipc_broker_start_async(IPC_BROKER_PIPE);

    return g_ipc_broker == nullptr ? 1 : 0;
}

void Worker_IPCBroker::stop()
{
    ipc_broker_stop(g_ipc_broker);
}