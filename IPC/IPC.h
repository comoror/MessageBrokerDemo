#pragma once

typedef void (*PIPC_CLIENT_ON_CONNECT) ();
typedef void (*PIPC_CLIENT_ON_DISCONNECT) ();
typedef void (*PIPC_CLIENT_ON_MESSAGE) (void* inBuf, size_t bufSize);

extern "C"
{
    //return: pointer to IPCClient instance, nullptr if failed
    void* ipc_client_start(const char* pipe_name,
        unsigned short client_id, 
        PIPC_CLIENT_ON_MESSAGE onMessage,
        PIPC_CLIENT_ON_CONNECT onConnect = nullptr,
        PIPC_CLIENT_ON_DISCONNECT onDisconnect = nullptr);

    void ipc_client_stop(void* pClient);

    int ipc_client_send(void* pClient,
        unsigned short srcID,
        unsigned short dstID,
        unsigned short msgType,
        void* data,
        unsigned short data_len);

    int ipc_client_broadcast(void* pClient, 
        unsigned short srcID,
        unsigned short msgType,
        void* data, 
        unsigned short data_len);

    int ipc_client_register_msg(void* pClient, 
        unsigned short msgType);

    //return: pointer to IPCServerBroker instance, nullptr if failed
    void* ipc_broker_start(const char* pipe_name);
    void* ipc_broker_start_async(const char* pipe_name);
    void ipc_broker_stop(void* pBroker);
}