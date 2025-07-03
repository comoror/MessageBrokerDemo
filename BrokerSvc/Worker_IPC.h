#pragma once

#include "..\IPC\IPCClient.h"

namespace Worker_IPC
{
    void start();
    void stop();

    void send(IpcMessagePtr msg);
};

