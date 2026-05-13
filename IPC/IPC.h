#pragma once

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

// ================================================================
// Part 1: Raw Named Pipe API (no protocol, no broker, raw bytes)
// ================================================================

typedef void* IPC_PIPE_SERVER_HANDLE;
typedef void* IPC_PIPE_CLIENT_HANDLE;

// Server callbacks (with user context)
typedef void (*PIPC_PIPE_ON_MESSAGE) (void* context, unsigned long pipeIndex, void* data, size_t size);
typedef void (*PIPC_PIPE_ON_CONNECT) (void* context, unsigned long pipeIndex);
typedef void (*PIPC_PIPE_ON_DISCONNECT) (void* context, unsigned long pipeIndex);

// Client callbacks
typedef void (*PIPC_PIPE_CLIENT_ON_MESSAGE) (void* data, size_t size);
typedef void (*PIPC_PIPE_CLIENT_ON_CONNECT) ();
typedef void (*PIPC_PIPE_CLIENT_ON_DISCONNECT) ();

#ifdef __cplusplus
extern "C"
{
#endif

    // -- Pipe Server --

    IPC_PIPE_SERVER_HANDLE ipc_pipe_server_start(const char* pipe_name,
        PIPC_PIPE_ON_MESSAGE onMessage,
        PIPC_PIPE_ON_CONNECT onConnect,
        PIPC_PIPE_ON_DISCONNECT onDisconnect,
        void* context);

    IPC_RESULT ipc_pipe_server_send(IPC_PIPE_SERVER_HANDLE hServer,
        unsigned long pipeIndex,
        void* data, size_t data_size);

    void ipc_pipe_server_broadcast(IPC_PIPE_SERVER_HANDLE hServer,
        void* data, size_t data_size);

    void ipc_pipe_server_disconnect_client(IPC_PIPE_SERVER_HANDLE hServer,
        unsigned long pipeIndex);

    void ipc_pipe_server_stop(IPC_PIPE_SERVER_HANDLE hServer);

    // -- Pipe Client --

    IPC_PIPE_CLIENT_HANDLE ipc_pipe_client_connect(const char* pipe_name,
        PIPC_PIPE_CLIENT_ON_MESSAGE onMessage,
        PIPC_PIPE_CLIENT_ON_CONNECT onConnect,
        PIPC_PIPE_CLIENT_ON_DISCONNECT onDisconnect);

    IPC_RESULT ipc_pipe_client_send(IPC_PIPE_CLIENT_HANDLE hClient,
        void* data, size_t data_size);

    void ipc_pipe_client_disconnect(IPC_PIPE_CLIENT_HANDLE hClient);

#ifdef __cplusplus
}
#endif

// ================================================================
// Part 2: IPC Broker API (with IPC protocol and message routing)
// ================================================================

typedef void* IPC_CLIENT_HANDLE;
typedef void* IPC_BROKER_HANDLE;

// Broker client callbacks
typedef void (*PIPC_CLIENT_ON_CONNECT) ();
typedef void (*PIPC_CLIENT_ON_DISCONNECT) ();
typedef void (*PIPC_CLIENT_ON_MESSAGE) (void* inBuf, size_t bufSize);

// Broker server callbacks
typedef bool (*PIPC_BROKER_ON_CLIENT_CONNECT) (void* hPipe);
typedef bool (*PIPC_BROKER_ON_CLIENT_AUTH) (void* hPipe, unsigned short clientId);
typedef void (*PIPC_BROKER_ON_CLIENT_DISCONNECT) (unsigned short clientId);

#ifdef __cplusplus
extern "C"
{
#endif

    // -- Broker Client --

    IPC_CLIENT_HANDLE ipc_client_start(const char* pipe_name,
        unsigned short client_id, 
        PIPC_CLIENT_ON_MESSAGE onMessage,
        PIPC_CLIENT_ON_CONNECT onConnect,
        PIPC_CLIENT_ON_DISCONNECT onDisconnect);

    void ipc_client_stop(IPC_CLIENT_HANDLE pClient);

    IPC_RESULT ipc_client_send(IPC_CLIENT_HANDLE pClient,
        unsigned short dstID,
        unsigned short msgType,
        void* data,
        unsigned short data_len);

    IPC_RESULT ipc_client_broadcast(IPC_CLIENT_HANDLE pClient, 
        unsigned short msgType,
        void* data, 
        unsigned short data_len);

    IPC_RESULT ipc_client_register_msg(IPC_CLIENT_HANDLE pClient, 
        unsigned short msgType);

    // -- Broker --

    IPC_BROKER_HANDLE ipc_broker_start(const char* pipe_name,
        PIPC_BROKER_ON_CLIENT_CONNECT onConnect,
        PIPC_BROKER_ON_CLIENT_AUTH onAuth,
        PIPC_BROKER_ON_CLIENT_DISCONNECT onDisconnect);

    void ipc_broker_stop(IPC_BROKER_HANDLE pBroker);

#ifdef __cplusplus
}
#endif
