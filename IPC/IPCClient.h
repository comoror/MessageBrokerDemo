#pragma once

#include "IPCMessage.h"

#ifdef _WIN32
#include "CNamedPipeClient.h"
#include <cassert>
class IPCClient
{
public:
    IPCClient(unsigned short clientId);
    ~IPCClient();

    bool Connect(const char* pipeName, PPIPE_CLIENT_ON_MESSAGE onMessage,
        PPIPE_CLIENT_ON_CONNECT onConnect = nullptr,
        PPIPE_CLIENT_ON_DISCONNECT onDisconnect = nullptr);
    void Disconnect();

    int Send(IpcMessage* msg);
    int RegisterMessage(unsigned short type);

private:
    unsigned short  mClientId = 0;     // Identifier for the client
    CNamedPipeClient* pClient = nullptr;

    void RegisterClient(unsigned short clientId);
};
#else
// For non-Windows platforms, you can define a dummy IPCClient or implement your own IPC mechanism
#endif
