// IPC.cpp : 定义静态库的函数。
//

#include "pch.h"
#include "framework.h"
#include "IPC.h"
#include "IPCMessage.h"
#include "IPCClient.h"

void* ipc_client_start(const char* pipe_name,
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

void ipc_client_stop(void* pClient)
{
    IPCClient* pIpcClient = (IPCClient*)pClient;
    if (pIpcClient)
    {
        pIpcClient->Disconnect();
        delete pIpcClient;
    }
}

int ipc_client_send(void* pClient, 
    unsigned short srcID, 
    unsigned short dstID, 
    unsigned short msgType, 
    void* data, 
    unsigned short data_len)
{
    IPCClient* pIpcClient = (IPCClient*)pClient;

    if (!pIpcClient)
    {
        return -1;
    }

    // 验证数据大小
    constexpr size_t MAX_DATA_SIZE = sizeof(IpcMessage::Data);
    if (data_len > MAX_DATA_SIZE)
    {
        DBG_ERROR("Data size %u exceeds maximum allowed size %zu", data_len, MAX_DATA_SIZE);
        return -1;
    }

    try
    {
        // 在栈上创建对象，Send 方法会立即发送数据
        IpcMessage message(srcID, dstID, msgType, data, data_len);
        return pIpcClient->Send(&message);
    }
    catch (const std::exception& e)
    {
        DBG_ERROR("Failed to send message: %s", e.what());
        return -1;
    }
}

int ipc_client_broadcast(void* pClient, 
    unsigned short srcID,
    unsigned short msgType, 
    void* data, 
    unsigned short data_len)
{
    return ipc_client_send(pClient, srcID, IPC_BROADCAST, msgType, data, data_len);
}

int ipc_client_register_msg(void* pClient, unsigned short msgType)
{
    IPCClient* pIpcClient = (IPCClient*)pClient;

    if (!pIpcClient)
    {
        return -1;
    }

    // 重试机制：尝试注册消息类型
    int retry = 5;
    do {
        int result = pIpcClient->RegisterMessage(msgType);
        if (result >= 0)  // 成功
        {
            return result;
        }
        Sleep(100);
    } while (--retry > 0);
    
    return -2;  // 所有重试都失败
}

//////////////////////////////////////////////////////////////////
#include "IPCServerBroker.h"

void* ipc_broker_start(const char* pipe_name)
{
    IPCServerBroker* pServerBroker = nullptr;
    
    try
    {
    	pServerBroker = new IPCServerBroker();
        pServerBroker->RunBroker(pipe_name);
    	return pServerBroker;
	}
    catch (const std::exception& e)
    {
        DBG_ERROR("Failed to start broker: %s", e.what());
        delete pServerBroker;
        return nullptr;
    }
}

void* ipc_broker_start_async(const char* pipe_name)
{
    IPCServerBroker* pServerBroker = nullptr;
    
    try
    {
    	pServerBroker = new IPCServerBroker();
        pServerBroker->RunBrokerAsync(pipe_name);
        return pServerBroker;
    }
    catch (const std::exception& e)
    {
        DBG_ERROR("Failed to start broker async: %s", e.what());
        delete pServerBroker;
        return nullptr;
    }
}

void ipc_broker_stop(void* pBroker)
{
    IPCServerBroker* pServerBroker = (IPCServerBroker*)pBroker;
    if (pServerBroker)
    {
        pServerBroker->StopBroker();
        delete pServerBroker;
    }
}
