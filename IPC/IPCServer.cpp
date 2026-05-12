#include "pch.h"
#include "IPCServer.h"

bool IPCServer::Listen(const char* pipeName,
    PPIPE_SERVER_ON_MESSAGE onMessage,
    PPIPE_SERVER_ON_CONNECT onConnect,
    PPIPE_SERVER_ON_DISCONNECT onDisconnect)
{
    // 如果已有服务器实例，先清理
    if (pServer)
    {
        delete pServer;
        pServer = nullptr;
    }

#ifdef UNICODE
    wchar_t pipeNameW[MAX_PATH];
    mbstowcs_s(nullptr, pipeNameW, MAX_PATH, pipeName, _TRUNCATE);
    pServer = new CNamedPipeServer(pipeNameW, onMessage, onConnect, onDisconnect);
#else
    pServer = new CNamedPipeServer((char*)pipeName, onMessage, onConnect, onDisconnect);
#endif

    return (pServer != nullptr) ? true : false;
}

void IPCServer::SendData(unsigned long index, IpcMessage* msg)
{
    if (pServer)
    {
        pServer->SendData(index, msg, msg->header.Size);
    }
}

void IPCServer::Start()
{
    if (pServer)
    {
        pServer->Run();
    }
}

void IPCServer::Stop()
{
    if (pServer)
    {
        pServer->Stop();
        delete pServer;
        pServer = nullptr;
    }
}

void IPCServer::DisconnectClient(unsigned long index)
{
    if (pServer)
    {
        pServer->ForceDisconnect(static_cast<DWORD>(index));
    }
}

void* IPCServer::GetClientHandle(unsigned long index)
{
    if (pServer)
    {
        return pServer->GetPipeHandle(static_cast<DWORD>(index));
    }
    return INVALID_HANDLE_VALUE;
}
