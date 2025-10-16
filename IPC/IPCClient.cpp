#include "pch.h"
#include "IPCClient.h"

IPCClient::IPCClient(unsigned short clientId)
    : mClientId(clientId), pClient(nullptr) {
}

IPCClient::~IPCClient() {
    Disconnect();
}

bool IPCClient::Connect(const char* pipeName, PPIPE_CLIENT_ON_MESSAGE onMessage,
    PPIPE_CLIENT_ON_CONNECT onConnect,
    PPIPE_CLIENT_ON_DISCONNECT onDisconnect)
{
    pClient = new CNamedPipeClient();
#ifdef UNICODE
    wchar_t pipeNameW[MAX_PATH];
    mbstowcs_s(nullptr, pipeNameW, MAX_PATH, pipeName, _TRUNCATE);
    if (pClient->Connect(pipeNameW, onMessage, onConnect, onDisconnect) == ERROR_SUCCESS)
#else
    if (pClient->Connect(pipeName, onMessage, onConnect, onDisconnect) == ERROR_SUCCESS)
#endif
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
}

void IPCClient::Disconnect()
{
    if (pClient)
    {
        pClient->Disconnect();
        delete pClient;
        pClient = nullptr;
    }
}

int IPCClient::Send(IpcMessage* msg)
{
    if (pClient)
    {
        msg->header.SrcId = mClientId; // Set the source ID to the client's ID
        return pClient->SendData(msg, msg->header.Size);
    }
    return -1;
}

int IPCClient::RegisterMessage(unsigned short type)
{
    if (pClient)
    {
        IpcMessage* pMsg = new IpcMessage(mClientId, 0, type, nullptr, 0);
        DWORD dwRet = pClient->SendData(pMsg, pMsg->header.Size);
        delete pMsg; // Clean up the message after sending
        return dwRet;
    }
    return -1;
}

void IPCClient::RegisterClient(unsigned short clientId)
{
    if (pClient)
    {
        IpcMessage* pMsg = new IpcMessage(mClientId, 0, 0, nullptr, 0);
        pClient->SendData(pMsg, pMsg->header.Size);
        delete pMsg; // Clean up the message after sending
    }
}
