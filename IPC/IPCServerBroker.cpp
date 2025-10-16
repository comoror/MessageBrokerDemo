#include "pch.h"
#include "IPCServerBroker.h"

IPCServerBroker* IPCServerBroker::pThis = nullptr;

IPCServerBroker::IPCServerBroker() : server(nullptr)
{
    pThis = this;
}

IPCServerBroker::~IPCServerBroker()
{
    StopBroker();
}

void IPCServerBroker::ClientAdd(unsigned long index)
{
    std::lock_guard<std::mutex> lock(mMutex);

    // Check if client already exists
    auto it = std::find_if(mClients.begin(), mClients.end(),
        [index](const ClientInfo& client) { return client.ClientIndex == index; });
    if (it != mClients.end())
    {
        DBG_INFO("Client with index %lu already exists.", index);
        return;
    }

    ClientInfo info;
    info.ClientIndex = index;
    info.ClientId = 0;
    mClients.push_back(info);
    DBG_INFO("Client with index %lu added.", index);
}
void IPCServerBroker::ClientDelete(unsigned long index)
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
void IPCServerBroker::ClientUpdate(unsigned long index, unsigned short srcId)
{
    DBG_INFO("Client [%04d] with pipe index %lu updated.", srcId, index);
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = std::find_if(mClients.begin(), mClients.end(),
        [index](const ClientInfo& client) { return client.ClientIndex == index; });
    if (it != mClients.end())
    {
        it->ClientId = srcId;
    }
}

// Register client to a specific message type
void IPCServerBroker::MessageRegister(unsigned long index, unsigned short type)
{
    DBG_INFO("Client with index %lu registered to message type 0x%04X.", index, type);
    std::lock_guard<std::mutex> lock(mMutex);

    auto it = std::find_if(mClients.begin(), mClients.end(),
        [index](const ClientInfo& client) { return client.ClientIndex == index; });
    if (it != mClients.end())
    {
        auto msgIt = mMessages.find(type);
        if(msgIt != mMessages.end())
        {
            // Check if client already registered for this message type
            auto clientIt = std::find_if(msgIt->second.begin(), msgIt->second.end(),
                [index](const ClientInfo& client) { return client.ClientIndex == index; });
            if (clientIt != msgIt->second.end())
            {
                DBG_INFO("Client with index %lu already registered for message type 0x%04X.", index, type);
                return;
            }
        }
        mMessages[type].push_back(*it);
    }
}

void IPCServerBroker::Send(unsigned long index, IpcMessage* msg)
{
    if (server)
    {
        server->SendData(index, msg);
    }
}

void IPCServerBroker::SendError(unsigned long index, unsigned short srcId, void* data, size_t data_size)
{
    if (server)
    {
        IpcMessage* msg = new IpcMessage(0, srcId, IPC_ERROR_DST_NOT_ONLINE, data, data_size);
        server->SendData(index, msg);
        delete msg;
    }
}

// Broadcast message to all clients registered to a specific message type
void IPCServerBroker::Broadcast(unsigned short type, IpcMessage* msg)
{
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = mMessages.find(type);
    if (it != mMessages.end())
    {
        DBG_INFO("Broadcasting message type 0x%04X to %zu clients.", type, it->second.size());
        for (auto& client : it->second)
        {
            Send(client.ClientIndex, msg);
        }
    }
    else
    {
        DBG_INFO("No clients registered for message type 0x%04X.", type);
    }
}

void IPCServerBroker::SendToClient(unsigned short dstId, IpcMessage* msg)
{
    if (!server)
    {
        DBG_ERROR("IPC Server is not running. Cannot send message to client.");
        return;
    }

    // Find client with the specified ID
    std::lock_guard<std::mutex> lock(mMutex);
    auto it = std::find_if(mClients.begin(), mClients.end(),
        [dstId](const ClientInfo& client) { return client.ClientId == dstId; });

    if (it != mClients.end())
    {
        DBG_INFO("Sending message to client with ID 0x%04X.", dstId);
        Send(it->ClientIndex, msg);
    }
    else
    {
        DBG_INFO("No client with ID 0x%04X found.", dstId);
    }
}

void IPCServerBroker::OnServerConnect(unsigned long index)
{
    DBG_INFO("Client connected with index: %lu", index);
    pThis->ClientAdd(index);
}

void IPCServerBroker::OnServerDisconnect(unsigned long index)
{
    DBG_INFO("Client disconnected with index: %lu", index);
    pThis->ClientDelete(index);
}

void IPCServerBroker::OnServerMessage(unsigned long index, void* data, size_t data_size)
{
    IpcMessage* msg = (IpcMessage*)data;

    // Validate the message
    if (!msg->IsValid())
    {
        DBG_INFO("Invalid message received from client index %lu.", index);
        return;
    }

    unsigned short srcId = msg->header.SrcId;
    unsigned short dstId = msg->header.DstId;
    unsigned short type = msg->header.Type;

    if (dstId == IPC_CONTROL)
    {
        if (type == 0)  //an identifier for a client
        {
            // Update client information
            pThis->ClientUpdate(index, srcId);
        }
        else  // Register client to msg type
        {
            // Register client to msg type
            pThis->MessageRegister(index, type);
        }

    }
    else if (dstId == IPC_BROADCAST)
    {
        DBG_INFO("Broadcasting message type 0x%04X from client index %lu.", type, index);
        // Broadcast to all clients registered to msg type
        pThis->Broadcast(type, msg);
    }
    else
    {
        DBG_INFO("Sending message type 0x%04X from client index %lu to specific client with ID 0x%04X.", type, index, dstId);
        // Send to a specific client
        // Find client with the specified ID and send
        int index_dst = -1;
        for (const auto& client : pThis->mClients)
        {
            if (client.ClientId == dstId)
            {
                index_dst = client.ClientIndex;
                break;
            }
        }
        if (index_dst == -1)
        {
            DBG_INFO("No client with ID 0x%04X found.", dstId);
            pThis->SendError(index, srcId, data, data_size);

            return;
        }
        pThis->Send(index_dst, msg);
    }
}

void IPCServerBroker::RunBroker(const char* serverName)
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

    server->Start();
}

void IPCServerBroker::RunBrokerAsync(const char* serverName)
{
    std::thread([this, serverName]() {
        RunBroker(serverName);
        }).detach();
}

void IPCServerBroker::StopBroker()
{
    if (server)
    {
        server->Stop();
        delete server;
        server = nullptr;
    }
}
