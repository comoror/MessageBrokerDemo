#pragma once

#include "..\IPC\IPCServerBroker.h"

namespace Worker_Broker
{
    void start();
    void stop();

    void broadcast(IpcMessagePtr msg);
    void send(IpcMessagePtr msg, unsigned short dstId);
};

