#pragma once

#include "IPC.h"

#ifdef _WIN32
#include "CNamedPipeClient.h"
class IPCClient
{
public:
    IPCClient(unsigned short clientId)
        : mClientId(clientId), pClient(nullptr) {
    }

    ~IPCClient() {
        Disconnect();
    }

    bool Connect(const char* pipeName, PPIPE_CLIENT_ON_MESSAGE onMessage,
        PPIPE_CLIENT_ON_CONNECT onConnect = nullptr,
        PPIPE_CLIENT_ON_DISCONNECT onDisconnect = nullptr)
    {
#ifdef UNICODE
        wchar_t pipeNameW[MAX_PATH];
        mbstowcs_s(nullptr, pipeNameW, MAX_PATH, pipeName, _TRUNCATE);
        pClient = new CNamedPipeClient();
        if (pClient->Connect(pipeNameW, onMessage, onConnect, onDisconnect) == ERROR_SUCCESS)
        {
            // Register the client with the server
            RegisterClient(mClientId);
            return true;
        }
        else
        {
            delete pClient;
            pClient = nullptr;
            return false;
        }

#else
        pClient = new CNamedPipeClient();
        return (pClient->Connect(pipeName, onMessage, onConnect, onDisconnect) == ERROR_SUCCESS);
#endif
    }

    void Disconnect()
    {
        if (pClient)
        {
            pClient->Disconnect();
            delete pClient;
            pClient = nullptr;
        }
    }

    void Send(IpcMessage* msg)
    {
        if (pClient)
        {
            msg->header.SrcId = mClientId; // Set the source ID to the client's ID
            pClient->SendData(msg, sizeof(IpcMessage) + msg->header.DataSize - 1);
        }
    }

    void RegisterMessage(unsigned short type)
    {
        if (pClient)
        {
            IpcMessage msg;
            msg.header.SrcId = mClientId;
            msg.header.DstId = IPC_DST_REGISTER_MESSAGE;
            msg.header.Type = type;
            msg.header.DataSize = 0;
            pClient->SendData(&msg, sizeof(IpcMessage) + sizeof(type) - 1);
        }
    }
private:
    unsigned short  mClientId = 0;     // Identifier for the client
    CNamedPipeClient* pClient = nullptr;

    void RegisterClient(unsigned short clientId)
    {
        if (pClient)
        {
            IpcMessage msg;
            msg.header.SrcId = mClientId;
            msg.header.DstId = IPC_DST_REGISTER_CLIENT;
            msg.header.Type = clientId; // Use the client ID as the type
            msg.header.DataSize = 0;
            pClient->SendData(&msg, sizeof(IpcMessage) + sizeof(clientId) - 1);
        }
    }

};
#else
// For non-Windows platforms, you can define a dummy IPCClient or implement your own IPC mechanism
#endif
