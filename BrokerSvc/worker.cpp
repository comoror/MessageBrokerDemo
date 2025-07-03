#include "worker.h"
#include "..\IPC\IPCServerBroker.h"
#include "Log.h"
#include "Worker_IPC.h"
#include "Worker_Broker.h"

void worker_start()
{
    // Initialize logging
    log_init("BrokerSvc", "C:\\ProgramData\\BrokerSvc\\Svc.log", LEVEL_TRACE);

    Worker_Broker::start();
    Worker_IPC::start();
}

void worker_stop()
{
    Worker_IPC::stop();
    Worker_Broker::stop();
}
