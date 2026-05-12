#pragma once

#include <map>
#include <vector>
#include <mutex>
#include "IPCMessage.h"
#include "IPCServer.h"
#include "IPC.h"

typedef struct
{
    unsigned long   ClientIndex;
    unsigned short  ClientId;
}ClientInfo;

class IPCServerBroker
{
public:
    IPCServerBroker();
    ~IPCServerBroker();

private:
    IPCServer* server = nullptr;
    PIPC_BROKER_ON_CLIENT_CONNECT m_pOnConnect = nullptr;
    PIPC_BROKER_ON_CLIENT_AUTH m_pOnAuth = nullptr;
    PIPC_BROKER_ON_CLIENT_DISCONNECT m_pOnClientDisconnect = nullptr;
    std::thread m_brokerThread;

    std::mutex mMutex;
    std::vector<ClientInfo> mClients;
    std::map<unsigned short, std::vector<ClientInfo>> mMessages;

    void ClientAdd(unsigned long index);
    void ClientDelete(unsigned long index);
    void ClientUpdate(unsigned long index, unsigned short srcId);

    void MessageRegister(unsigned long index, unsigned short type);
    void Send(unsigned long index, IpcMessage* msg);
    void SendError(unsigned long index, unsigned short srcId);

    void SendInvalid(unsigned long index);
    void SendTooLarge(unsigned long index, unsigned short srcId);
    void SendKick(unsigned long index, unsigned short clientId);
    void DisconnectClient(unsigned long index);

    static void OnServerConnect(void* pContext, unsigned long index);
    static void OnServerDisconnect(void* pContext, unsigned long index);
    static void OnServerMessage(void* pContext, unsigned long index, void* msg, size_t data_size);

public:
    void RunBroker(const char* serverName, PIPC_BROKER_ON_CLIENT_CONNECT onConnect = nullptr, PIPC_BROKER_ON_CLIENT_AUTH onAuth = nullptr);
    void RunBrokerAsync(const char* serverName, PIPC_BROKER_ON_CLIENT_CONNECT onConnect = nullptr, PIPC_BROKER_ON_CLIENT_AUTH onAuth = nullptr);
    void SetOnClientDisconnect(PIPC_BROKER_ON_CLIENT_DISCONNECT onDisconnect);
    void StopBroker();
    void Broadcast(unsigned short type, IpcMessage* msg);
    void SendToClient(unsigned short dstId, IpcMessage* msg);
};
