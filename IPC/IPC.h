#pragma once

typedef void* IPC_CLIENT_HANDLE;
typedef void* IPC_BROKER_HANDLE;

typedef void (*PIPC_CLIENT_ON_CONNECT) ();
typedef void (*PIPC_CLIENT_ON_DISCONNECT) ();
typedef void (*PIPC_CLIENT_ON_MESSAGE) (void* inBuf, size_t bufSize);

// Broker auth callback: receives pipe HANDLE, return true to allow, false to reject
typedef bool (*PIPC_BROKER_ON_AUTH) (void* hPipe);

// Error codes
enum IPC_RESULT : int
{
    IPC_OK              =  0,
    IPC_ERR_INVALID_PARAM = -1,
    IPC_ERR_NOT_CONNECTED = -2,
    IPC_ERR_SEND_FAILED   = -3,
    IPC_ERR_DATA_TOO_LARGE = -4,
    IPC_ERR_RETRY_FAILED  = -5,
};

extern "C"
{
    //return: IPC_CLIENT_HANDLE, nullptr if failed
    IPC_CLIENT_HANDLE ipc_client_start(const char* pipe_name,
        unsigned short client_id, 
        PIPC_CLIENT_ON_MESSAGE onMessage,
        PIPC_CLIENT_ON_CONNECT onConnect = nullptr,
        PIPC_CLIENT_ON_DISCONNECT onDisconnect = nullptr);

    void ipc_client_stop(IPC_CLIENT_HANDLE pClient);

    int ipc_client_send(IPC_CLIENT_HANDLE pClient,
        unsigned short dstID,
        unsigned short msgType,
        void* data,
        unsigned short data_len);

    int ipc_client_broadcast(IPC_CLIENT_HANDLE pClient, 
        unsigned short msgType,
        void* data, 
        unsigned short data_len);

    int ipc_client_register_msg(IPC_CLIENT_HANDLE pClient, 
        unsigned short msgType);

    //return: IPC_BROKER_HANDLE, nullptr if failed
    IPC_BROKER_HANDLE ipc_broker_start(const char* pipe_name, PIPC_BROKER_ON_AUTH onAuth = nullptr);
    IPC_BROKER_HANDLE ipc_broker_start_async(const char* pipe_name, PIPC_BROKER_ON_AUTH onAuth = nullptr);
    void ipc_broker_stop(IPC_BROKER_HANDLE pBroker);
}