#pragma once

#include "IPC.h"

#ifdef _WIN32
// For Windows platforms, you can use the CNamedPipeServer class from CNamedPipeIPC.h
#include "CNamedPipeServer.h"

class IPCServer
{
public:
    bool Listen(const char* pipeName,
        PPIPE_SERVER_ON_MESSAGE onMessage,
        PPIPE_SERVER_ON_CONNECT onConnect,
        PPIPE_SERVER_ON_DISCONNECT onDisconnect)
    {
#ifdef UNICODE
        wchar_t pipeNameW[MAX_PATH];
        mbstowcs_s(nullptr, pipeNameW, MAX_PATH, pipeName, _TRUNCATE);
        pServer = new CNamedPipeServer(pipeNameW, onMessage, onConnect, onDisconnect);
#else
        pServer = new CNamedPipeServer((const char*)pipeName, onMessage, onConnect, onDisconnect);
#endif

        return (pServer != nullptr) ? true : false;
    }

    void SendData(unsigned long index, IpcMessage* msg)
    {
        if (pServer)
        {
            pServer->SendData(index, msg, sizeof(IpcMessage) + msg->header.DataSize - 1);
        }
    }

    void Start()
    {
        if (pServer)
        {
            pServer->Run();
        }
    }

    void Stop()
    {
        if (pServer)
        {
            pServer->Stop();
            delete pServer;
            pServer = nullptr;
        }
    }

private:
    CNamedPipeServer* pServer = nullptr;
};
#else
// For non-Windows platforms, you can define a dummy IPCServer or implement your own IPC mechanism
#endif