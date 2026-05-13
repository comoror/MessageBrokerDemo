// IPC.cpp : 定义静态库的函数。
//

#include "pch.h"
#include "framework.h"
#include "IPC.h"
#include "IPCMessage.h"
#include "IPCClient.h"

IPC_CLIENT_HANDLE ipc_client_start(const char* pipe_name,
    unsigned short client_id,
    PIPC_CLIENT_ON_MESSAGE onMessage,
    PIPC_CLIENT_ON_CONNECT onConnect,
    PIPC_CLIENT_ON_DISCONNECT onDisconnect)
{
    IPCClient* pIpcClient = nullptr;
    
    try
    {
    	pIpcClient = new IPCClient(client_id);

        if (pIpcClient->Connect(pipe_name, onMessage, onConnect, onDisconnect))
        {
            DBG_INFO("Connect success");
            return pIpcClient;
        }
        else
        {
            DBG_INFO("Connect failed");
            delete pIpcClient;
            return nullptr;
        }
    }
    catch (const std::exception& e)
    {
        DBG_ERROR("Failed to start client: %s", e.what());
        delete pIpcClient;
        return nullptr;
	}
}

void ipc_client_stop(IPC_CLIENT_HANDLE pClient)
{
    IPCClient* pIpcClient = (IPCClient*)pClient;
    if (pIpcClient)
    {
        pIpcClient->Disconnect();
        delete pIpcClient;
    }
}

IPC_RESULT ipc_client_send(IPC_CLIENT_HANDLE pClient,
    unsigned short dstID,
    unsigned short msgType,
    void* data,
    unsigned short data_len)
{
    IPCClient* pIpcClient = (IPCClient*)pClient;

    if (!pIpcClient)
    {
        return IPC_ERR_INVALID_PARAM;
    }

    // 验证数据大小
    constexpr size_t MAX_DATA_SIZE = sizeof(IpcMessage::Data);
    if (data_len > MAX_DATA_SIZE)
    {
        DBG_ERROR("Data size %u exceeds maximum allowed size %zu", data_len, MAX_DATA_SIZE);
        return IPC_ERR_DATA_TOO_LARGE;
    }

    try
    {
        // srcId=0, broker will fill in the real srcId from registration
        IpcMessage message(0, dstID, msgType, data, data_len);
        int ret = pIpcClient->Send(&message);
        return (ret == 0) ? IPC_OK : IPC_ERR_SEND_FAILED;
    }
    catch (const std::exception& e)
    {
        DBG_ERROR("Failed to send message: %s", e.what());
        return IPC_ERR_SEND_FAILED;
    }
}

IPC_RESULT ipc_client_broadcast(IPC_CLIENT_HANDLE pClient,
    unsigned short msgType,
    void* data,
    unsigned short data_len)
{
    return ipc_client_send(pClient, IPC_BROADCAST, msgType, data, data_len);
}

IPC_RESULT ipc_client_register_msg(IPC_CLIENT_HANDLE pClient, unsigned short msgType)
{
    IPCClient* pIpcClient = (IPCClient*)pClient;

    if (!pIpcClient)
    {
        return IPC_ERR_INVALID_PARAM;
    }

    // 重试机制：尝试注册消息类型
    int retry = 5;
    do {
        int result = pIpcClient->RegisterMessage(msgType);
        if (result >= 0)  // 成功
        {
            return IPC_OK;
        }
        Sleep(100);
    } while (--retry > 0);
    
    return IPC_ERR_RETRY_FAILED;
}

//////////////////////////////////////////////////////////////////
#include "IPCServerBroker.h"

IPC_BROKER_HANDLE ipc_broker_start(const char* pipe_name,
    PIPC_BROKER_ON_CLIENT_CONNECT onConnect,
    PIPC_BROKER_ON_CLIENT_AUTH onAuth,
    PIPC_BROKER_ON_CLIENT_DISCONNECT onDisconnect)
{
    IPCServerBroker* pServerBroker = nullptr;
    
    try
    {
    	pServerBroker = new IPCServerBroker();
        pServerBroker->SetOnClientDisconnect(onDisconnect);
        pServerBroker->RunBrokerAsync(pipe_name, onConnect, onAuth);
        return pServerBroker;
    }
    catch (const std::exception& e)
    {
        DBG_ERROR("Failed to start broker: %s", e.what());
        delete pServerBroker;
        return nullptr;
    }
}

void ipc_broker_stop(IPC_BROKER_HANDLE pBroker)
{
    IPCServerBroker* pServerBroker = (IPCServerBroker*)pBroker;
    if (pServerBroker)
    {
        pServerBroker->StopBroker();
        delete pServerBroker;
    }
}

//////////////////////////////////////////////////////////////////
// Raw Named Pipe API
//////////////////////////////////////////////////////////////////
#include "CNamedPipeServer.h"
#include "CNamedPipeClient.h"

struct PipeServerWrapper
{
    CNamedPipeServer* pServer = nullptr;
    std::thread serverThread;

    ~PipeServerWrapper()
    {
        if (pServer)
        {
            pServer->Stop();
            if (serverThread.joinable())
            {
                serverThread.join();
            }
            delete pServer;
            pServer = nullptr;
        }
    }
};

IPC_PIPE_SERVER_HANDLE ipc_pipe_server_start(const char* pipe_name,
    PIPC_PIPE_ON_MESSAGE onMessage,
    PIPC_PIPE_ON_CONNECT onConnect,
    PIPC_PIPE_ON_DISCONNECT onDisconnect,
    void* context)
{
    if (!pipe_name || !onMessage)
    {
        return nullptr;
    }

    PipeServerWrapper* pWrapper = new(std::nothrow) PipeServerWrapper();
    if (!pWrapper)
    {
        return nullptr;
    }

#ifdef UNICODE
    wchar_t pipeNameW[MAX_PATH];
    mbstowcs_s(nullptr, pipeNameW, MAX_PATH, pipe_name, _TRUNCATE);
    pWrapper->pServer = new(std::nothrow) CNamedPipeServer(pipeNameW,
        (PPIPE_SERVER_ON_MESSAGE)onMessage,
        (PPIPE_SERVER_ON_CONNECT)onConnect,
        (PPIPE_SERVER_ON_DISCONNECT)onDisconnect,
        context);
#else
    pWrapper->pServer = new(std::nothrow) CNamedPipeServer(pipe_name,
        (PPIPE_SERVER_ON_MESSAGE)onMessage,
        (PPIPE_SERVER_ON_CONNECT)onConnect,
        (PPIPE_SERVER_ON_DISCONNECT)onDisconnect,
        context);
#endif

    if (!pWrapper->pServer)
    {
        delete pWrapper;
        return nullptr;
    }

    // Run server in background thread
    pWrapper->serverThread = std::thread([pWrapper]() {
        pWrapper->pServer->Run();
    });

    return pWrapper;
}

IPC_RESULT ipc_pipe_server_send(IPC_PIPE_SERVER_HANDLE hServer,
    unsigned long pipeIndex,
    void* data, size_t data_size)
{
    PipeServerWrapper* pWrapper = (PipeServerWrapper*)hServer;
    if (!pWrapper || !pWrapper->pServer || !data || data_size == 0)
    {
        return IPC_ERR_INVALID_PARAM;
    }

    DWORD ret = pWrapper->pServer->SendData((DWORD)pipeIndex, data, data_size);
    return (ret == 0) ? IPC_OK : IPC_ERR_SEND_FAILED;
}

void ipc_pipe_server_broadcast(IPC_PIPE_SERVER_HANDLE hServer,
    void* data, size_t data_size)
{
    PipeServerWrapper* pWrapper = (PipeServerWrapper*)hServer;
    if (!pWrapper || !pWrapper->pServer || !data || data_size == 0)
    {
        return;
    }
    pWrapper->pServer->BroadcastData(data, data_size);
}

void ipc_pipe_server_disconnect_client(IPC_PIPE_SERVER_HANDLE hServer,
    unsigned long pipeIndex)
{
    PipeServerWrapper* pWrapper = (PipeServerWrapper*)hServer;
    if (!pWrapper || !pWrapper->pServer)
    {
        return;
    }
    pWrapper->pServer->ForceDisconnect((DWORD)pipeIndex);
}

void ipc_pipe_server_stop(IPC_PIPE_SERVER_HANDLE hServer)
{
    PipeServerWrapper* pWrapper = (PipeServerWrapper*)hServer;
    if (pWrapper)
    {
        delete pWrapper;  // Destructor handles Stop + join + cleanup
    }
}

IPC_PIPE_CLIENT_HANDLE ipc_pipe_client_connect(const char* pipe_name,
    PIPC_PIPE_CLIENT_ON_MESSAGE onMessage,
    PIPC_PIPE_CLIENT_ON_CONNECT onConnect,
    PIPC_PIPE_CLIENT_ON_DISCONNECT onDisconnect)
{
    if (!pipe_name || !onMessage)
    {
        return nullptr;
    }

    CNamedPipeClient* pClient = new(std::nothrow) CNamedPipeClient();
    if (!pClient)
    {
        return nullptr;
    }

#ifdef UNICODE
    wchar_t pipeNameW[MAX_PATH];
    mbstowcs_s(nullptr, pipeNameW, MAX_PATH, pipe_name, _TRUNCATE);
    DWORD ret = pClient->Connect(pipeNameW,
        (PPIPE_CLIENT_ON_MESSAGE)onMessage,
        (PPIPE_CLIENT_ON_CONNECT)onConnect,
        (PPIPE_CLIENT_ON_DISCONNECT)onDisconnect);
#else
    DWORD ret = pClient->Connect(pipe_name,
        (PPIPE_CLIENT_ON_MESSAGE)onMessage,
        (PPIPE_CLIENT_ON_CONNECT)onConnect,
        (PPIPE_CLIENT_ON_DISCONNECT)onDisconnect);
#endif

    if (ret != ERROR_SUCCESS)
    {
        delete pClient;
        return nullptr;
    }

    return pClient;
}

IPC_RESULT ipc_pipe_client_send(IPC_PIPE_CLIENT_HANDLE hClient,
    void* data, size_t data_size)
{
    CNamedPipeClient* pClient = (CNamedPipeClient*)hClient;
    if (!pClient || !data || data_size == 0)
    {
        return IPC_ERR_INVALID_PARAM;
    }

    DWORD ret = pClient->SendData(data, (DWORD)data_size);
    return (ret == 0) ? IPC_OK : IPC_ERR_SEND_FAILED;
}

void ipc_pipe_client_disconnect(IPC_PIPE_CLIENT_HANDLE hClient)
{
    CNamedPipeClient* pClient = (CNamedPipeClient*)hClient;
    if (pClient)
    {
        pClient->Disconnect();
        delete pClient;
    }
}
