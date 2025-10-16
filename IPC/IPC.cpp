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
    
    pIpcClient = new IPCClient(client_id);

    if (pIpcClient)
    {
        if (pIpcClient->Connect(pipe_name, onMessage, onConnect, onDisconnect))
        {
            DBG_INFO("Connect success");
        }
        else
        {
            DBG_INFO("Connect failed");
            delete pIpcClient;
            pIpcClient = nullptr;
        }
    }

    return pIpcClient;
}

void ipc_client_stop(void* pClient)
{
    IPCClient* pIpcClient = (IPCClient*)pClient;
    if (pIpcClient)
    {
        pIpcClient->Disconnect();
        delete pIpcClient;
        pIpcClient = nullptr;
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

    if (pIpcClient)
    {
        return pIpcClient->Send(std::make_shared<IpcMessage>(srcID, dstID, msgType, data, data_len).get());
    }
    return -1;
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

    int retry = 5;
    do {
        if (pIpcClient)
        {
            return pIpcClient->RegisterMessage(msgType);
        }
        Sleep(100);
    } while (retry--);
    return -1;
}

//////////////////////////////////////////////////////////////////
#include "IPCServerBroker.h"

void* ipc_broker_start(const char* pipe_name)
{
    IPCServerBroker* pServerBroker = nullptr;
    pServerBroker = new IPCServerBroker();
    if (pServerBroker)
    {
        pServerBroker->RunBroker(pipe_name);
    }
    return pServerBroker;
}

void* ipc_broker_start_async(const char* pipe_name)
{
    IPCServerBroker* pServerBroker = nullptr;
    pServerBroker = new IPCServerBroker();
    if (pServerBroker)
    {
        pServerBroker->RunBrokerAsync(pipe_name);
    }
    return pServerBroker;
}

void ipc_broker_stop(void* pBroker)
{
    IPCServerBroker* pServerBroker = (IPCServerBroker*)pBroker;
    if (pServerBroker)
    {
        pServerBroker->StopBroker();
        delete pServerBroker;
        pServerBroker = nullptr;
    }
}
