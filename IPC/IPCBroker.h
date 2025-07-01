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
    IPCServerBroker() : server(nullptr)
    {
        pThis = this;
    }
    ~IPCServerBroker()
    {
        StopBroker();
    }

    IPCServerBroker(const IPCServerBroker&) = delete;
    IPCServerBroker& operator=(const IPCServerBroker&) = delete;

private:
    IPCServer* server = nullptr;

    std::mutex mMutex;
    std::vector<ClientInfo> mClients;
    std::map<unsigned short, std::vector<ClientInfo>> mMessages;

    void ClientAdd(unsigned long index)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        // Check if client already exists
        auto it = std::find_if(mClients.begin(), mClients.end(),
            [index](const ClientInfo& client) { return client.ClientIndex == index; });
        if (it != mClients.end())
        {
            return;
        }

        ClientInfo info;
        info.ClientIndex = index;
        info.ClientId = 0;
        mClients.push_back(info);
    }
    void ClientDelete(unsigned long index)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        //remove frome clients
        auto it = std::remove_if(mClients.begin(), mClients.end(),
            [index](const ClientInfo& client) { return client.ClientIndex == index; });
        if (it != mClients.end())
        {
            mClients.erase(it);
        }

        //remove from messages
        for (auto it = mMessages.begin(); it != mMessages.end();)
        {
            it->second.erase(std::remove_if(it->second.begin(), it->second.end(),
                [index](const ClientInfo& client) { return client.ClientIndex == index; }), it->second.end());
            if (it->second.empty())
            {
                it = mMessages.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
    void ClientUpdate(unsigned long index, unsigned short srcId)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = std::find_if(mClients.begin(), mClients.end(),
            [index](const ClientInfo& client) { return client.ClientIndex == index; });
        if (it != mClients.end())
        {
            it->ClientId = srcId;
        }
    }

    // Register client to a specific message type
    void MessageRegister(unsigned long index, unsigned short type)
    {
        std::lock_guard<std::mutex> lock(mMutex);

        auto it = std::find_if(mClients.begin(), mClients.end(),
            [index](const ClientInfo& client) { return client.ClientIndex == index; });
        if (it != mClients.end())
        {
            mMessages[type].push_back(*it);
        }
    }

    void Send(unsigned long index, IpcMessage* msg)
    {
        if (server)
        {
            server->SendData(index, msg);
        }
    }

    // Broadcast message to all clients registered to a specific message type
    void Broadcast(unsigned short type, IpcMessage* msg)
    {
        std::lock_guard<std::mutex> lock(mMutex);
        auto it = mMessages.find(type);
        if (it != mMessages.end())
        {
            for (auto& client : it->second)
            {
                Send(client.ClientIndex, msg);
            }
        }
    }

    static void OnServerConnect(unsigned long index)
    {
        pThis->ClientAdd(index);
    }

    static void OnServerDisconnect(unsigned long index)
    {
        pThis->ClientDelete(index);
    }

    static void OnServerMessage(unsigned long index, VOID* data)
    {
        IpcMessage* msg = (IpcMessage*)data;

        unsigned short srcId = msg->header.SrcId;
        unsigned short dstId = msg->header.DstId;
        unsigned short type = msg->header.Type;

        if (type == IPC_DST_REGISTER_CLIENT)  //an identifier for a client
        {
            if (srcId != 0x0000 && srcId != 0xFFFF)
            {
                // Update client information
                pThis->ClientUpdate(index, srcId);
            }
        }
        else if (type == IPC_DST_REGISTER_MESSAGE)
        {
            if (srcId != 0x0000 && srcId != 0xFFFF)
            {
                // Register client to msg type
                pThis->MessageRegister(index, dstId);
            }
        }
        else if (dstId == IPC_DST_BROADCAST)
        {
            // Broadcast to all clients registered to msg type
            pThis->Broadcast(type, msg);
        }
        else
        {
            // Send to a specific client
            pThis->Send(dstId, msg);
        }
    }

    static IPCServerBroker* pThis;

public:
    static IPCServerBroker& GetInstance()
    {
        static IPCServerBroker instance;
        return instance;
    }

    void StartBroker(const char* serverName)
    {
        if (server)
        {
            // If server already exists, stop it first
            server->Stop();
            delete server;
            server = nullptr;
        }

        // Create a new IPC server instance
        server = new IPCServer();
        if (!server)
        {
            throw std::runtime_error("Failed to create IPC server instance");
        }

        if (!server->Listen(serverName, OnServerMessage, OnServerConnect, OnServerDisconnect))
        {
            delete server;
            server = nullptr;
            throw std::runtime_error("Failed to start IPC server");
        }
    }

    void StopBroker()
    {
        if (server)
        {
            server->Stop();
            delete server;
            server = nullptr;
        }
    }
};

IPCServerBroker* IPCServerBroker::pThis = nullptr;
