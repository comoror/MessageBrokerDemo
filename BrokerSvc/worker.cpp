#include "worker.h"
#include "..\IPC\IPCServerBroker.h"
#include "Log.h"
#include "Worker_IPCBroker.h"

void worker_start()
{
    // Initialize logging
    log_init("BrokerSvc", "C:\\ProgramData\\BrokerSvc\\Svc.log", LEVEL_TRACE);

    if (Worker_IPCBroker::start())
    {
        LOG_ERROR("Failed to start IPC Broker.");
    }
    else
    {
        LOG_INFO("start IPC broker success");
    }
}

void worker_stop()
{
    Worker_IPCBroker::stop();
}
