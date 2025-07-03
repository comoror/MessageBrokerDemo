#include "Worker_Broker.h"
#include "Log.h"
#include <thread>

static std::thread work_thread;

static int thread_func()
{
    IPCServerBroker::GetInstance().RunBroker("\\\\.\\pipe\\DemoBroker");  //block

    return 0;
}

void Worker_Broker::start()
{
    // Start a thread
    work_thread = std::thread(thread_func);
}

void Worker_Broker::stop()
{
    IPCServerBroker::GetInstance().StopBroker();

    // Wait for thread to exit
    if (work_thread.joinable())
        work_thread.join();
}

void Worker_Broker::broadcast(IpcMessagePtr msg)
{
    if (!msg)
    {
        LOG_ERROR("Message is null, cannot broadcast.");
        return;
    }
    unsigned short type = msg->header.Type;
    if (type == 0)
    {
        LOG_ERROR("Message type is 0, cannot broadcast.");
        return;
    }
    LOG_INFO("Broadcasting message type 0x%04X.", type);
    IPCServerBroker::GetInstance().Broadcast(type, msg.get());
}

void Worker_Broker::send(IpcMessagePtr msg, unsigned short dstId)
{
    if (!msg)
    {
        LOG_ERROR("Message is null, cannot send.");
        return;
    }
    unsigned short type = msg->header.Type;
    if (type == 0)
    {
        LOG_ERROR("Message type is 0, cannot send.");
        return;
    }
    LOG_INFO("Sending message type 0x%04X to client with ID 0x%04X.", type, dstId);
    IPCServerBroker::GetInstance().SendToClient(dstId, msg.get());
}
