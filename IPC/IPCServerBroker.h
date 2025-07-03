#pragma once

#include <map>
#include <vector>
#include <mutex>
#include "IPC.h"
#include "IPCServer.h"

typedef struct
{
    unsigned long   ClientIndex;
    unsigned short  ClientId;
}ClientInfo;

class IPCServerBroker
{
private:
    IPCServerBroker();
    ~IPCServerBroker();
    IPCServerBroker(const IPCServerBroker&) = delete;
    IPCServerBroker& operator=(const IPCServerBroker&) = delete;

private:
    IPCServer* server = nullptr;

    std::mutex mMutex;
    std::vector<ClientInfo> mClients;
    std::map<unsigned short, std::vector<ClientInfo>> mMessages;

    static IPCServerBroker* pThis;

    void ClientAdd(unsigned long index);
    void ClientDelete(unsigned long index);
    void ClientUpdate(unsigned long index, unsigned short srcId);

    void MessageRegister(unsigned long index, unsigned short type);
    void Send(unsigned long index, IpcMessage* msg);

    static void OnServerConnect(unsigned long index);
    static void OnServerDisconnect(unsigned long index);
    static void OnServerMessage(unsigned long index, VOID* msg);

public:
    static IPCServerBroker& GetInstance()
    {
        static IPCServerBroker instance;
        return instance;
    }

    void RunBroker(const char* serverName);
    void StopBroker();
    void Broadcast(unsigned short type, IpcMessage* msg);
    void SendToClient(unsigned short dstId, IpcMessage* msg);
};