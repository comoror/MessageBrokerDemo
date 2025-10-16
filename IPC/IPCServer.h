#pragma once

#include "IPCMessage.h"

#ifdef _WIN32
// For Windows platforms, you can use the CNamedPipeServer class from CNamedPipeIPC.h
#include "CNamedPipeServer.h"

class IPCServer
{
public:
    bool Listen(const char* pipeName,
        PPIPE_SERVER_ON_MESSAGE onMessage,
        PPIPE_SERVER_ON_CONNECT onConnect,
        PPIPE_SERVER_ON_DISCONNECT onDisconnect);

    void SendData(unsigned long index, IpcMessage* msg);

    void Start();

    void Stop();

private:
    CNamedPipeServer* pServer = nullptr;
};
#else
// For non-Windows platforms, you can define a dummy IPCServer or implement your own IPC mechanism
#endif