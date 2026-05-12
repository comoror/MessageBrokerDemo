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
