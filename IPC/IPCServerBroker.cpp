#include "pch.h"
#include "IPCServerBroker.h"

IPCServerBroker::IPCServerBroker() : server(nullptr), m_pOnConnect(nullptr)
{
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

    //remove from clients
    auto it = std::remove_if(mClients.begin(), mClients.end(),
        [index](const ClientInfo& client) { return client.ClientIndex == index; });
    if (it != mClients.end())
    {
        mClients.erase(it, mClients.end());
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
    DBG_INFO("Client [0x%04X] with pipe index %lu updated.", srcId, index);

    unsigned long oldIndex = (unsigned long)-1;

    // Under lock: detect duplicate, update
    {
        std::lock_guard<std::mutex> lock(mMutex);

        // Check if another client already has this srcId (duplicate detection)
        auto dupIt = std::find_if(mClients.begin(), mClients.end(),
            [index, srcId](const ClientInfo& client) {
                return client.ClientId == srcId && client.ClientIndex != index;
            });

        if (dupIt != mClients.end())
        {
            // Found duplicate - record old index for kick
            oldIndex = dupIt->ClientIndex;
            DBG_WARN("WARN: Client [0x%04X] duplicate detected, kicking old pipe index %lu.", srcId, oldIndex);

            // Remove old client from mClients
            mClients.erase(dupIt);

            // Remove old client from mMessages
            for (auto msgIt = mMessages.begin(); msgIt != mMessages.end();)
            {
                msgIt->second.erase(std::remove_if(msgIt->second.begin(), msgIt->second.end(),
                    [oldIndex](const ClientInfo& client) { return client.ClientIndex == oldIndex; }), msgIt->second.end());
                if (msgIt->second.empty())
                {
                    msgIt = mMessages.erase(msgIt);
                }
                else
                {
                    msgIt++;
                }
            }
        }

        // Update the current client's ID
        auto it = std::find_if(mClients.begin(), mClients.end(),
            [index](const ClientInfo& client) { return client.ClientIndex == index; });
        if (it != mClients.end())
        {
            it->ClientId = srcId;
        }
    }

    // After lock released: send KICK and disconnect old pipe
    if (oldIndex != (unsigned long)-1)
    {
        SendKick(oldIndex, srcId);
        DisconnectClient(oldIndex);
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

void IPCServerBroker::SendError(unsigned long index, unsigned short srcId)
{
    if (server)
    {
        IpcMessage msg(0, srcId, IPC_MSG_DST_NOT_FOUND, nullptr, 0);
        server->SendData(index, &msg);
    }
}

void IPCServerBroker::SendAck(unsigned long index, unsigned short srcId, unsigned short dstId)
{
    if (server)
    {
        IpcMessage msg(0, srcId, IPC_MSG_ACK, &dstId, sizeof(dstId));
        server->SendData(index, &msg);
    }
}

void IPCServerBroker::SendInvalid(unsigned long index)
{
    if (server)
    {
        IpcMessage msg(0, 0, IPC_MSG_INVALID, nullptr, 0);
        server->SendData(index, &msg);
    }
}

void IPCServerBroker::SendTooLarge(unsigned long index, unsigned short srcId)
{
    if (server)
    {
        IpcMessage msg(0, srcId, IPC_MSG_TOO_LARGE, nullptr, 0);
        server->SendData(index, &msg);
    }
}

void IPCServerBroker::SendKick(unsigned long index, unsigned short clientId)
{
    if (server)
    {
        IpcMessage msg(0, clientId, IPC_MSG_KICK, nullptr, 0);
        server->SendData(index, &msg);
    }
}

void IPCServerBroker::DisconnectClient(unsigned long index)
{
    if (server)
    {
        server->DisconnectClient(index);
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

void IPCServerBroker::OnServerConnect(void* pContext, unsigned long index)
{
    IPCServerBroker* pThis = static_cast<IPCServerBroker*>(pContext);
    if (!pThis)
    {
        DBG_ERROR("IPCServerBroker instance is null");
        return;
    }
    DBG_INFO("Client connected with index: %lu", index);
    pThis->ClientAdd(index);

    // OnConnect callback - caller decides whether to allow or reject
    if (pThis->m_pOnConnect)
    {
        void* hPipe = pThis->server->GetClientHandle(index);
        if (!pThis->m_pOnConnect(hPipe))
        {
            DBG_WARN("WARN: Client rejected for pipe index %lu, disconnecting.", index);
            pThis->ClientDelete(index);
            pThis->DisconnectClient(index);
            return;
        }
    }
}

void IPCServerBroker::OnServerDisconnect(void* pContext, unsigned long index)
{
    IPCServerBroker* pThis = static_cast<IPCServerBroker*>(pContext);
    if (!pThis)
    {
        DBG_ERROR("IPCServerBroker instance is null");
        return;
    }
    DBG_INFO("Client disconnected with index: %lu", index);

    // Find client_id before deleting, for the disconnect callback
    unsigned short clientId = 0;
    if (pThis->m_pOnClientDisconnect)
    {
        std::lock_guard<std::mutex> lock(pThis->mMutex);
        auto it = std::find_if(pThis->mClients.begin(), pThis->mClients.end(),
            [index](const ClientInfo& client) { return client.ClientIndex == index; });
        if (it != pThis->mClients.end())
        {
            clientId = it->ClientId;
        }
    }

    pThis->ClientDelete(index);

    // Notify the host application
    if (pThis->m_pOnClientDisconnect && clientId != 0)
    {
        pThis->m_pOnClientDisconnect(clientId);
    }
}

void IPCServerBroker::OnServerMessage(void* pContext, unsigned long index, void* data, size_t data_size)
{
    IPCServerBroker* pThis = static_cast<IPCServerBroker*>(pContext);
    if (!pThis)
    {
        DBG_ERROR("IPCServerBroker instance is null");
        return;
    }

    // Check message size too small (must at least contain a header)
    if (data_size < sizeof(IPCHeader))
    {
        DBG_WARN("WARN: Message too small from client index %lu, size: %zu.", index, data_size);
        pThis->SendInvalid(index);
        return;
    }

    // Check message size too large
    if (data_size > sizeof(IpcMessage))
    {
        DBG_WARN("WARN: Message too large from client index %lu, size: %zu.", index, data_size);
        // Header is guaranteed accessible (passed minimum size check above)
        IpcMessage* oversizeMsg = (IpcMessage*)data;
        pThis->SendTooLarge(index, oversizeMsg->header.SrcId);
        return;
    }

    IpcMessage* msg = (IpcMessage*)data;

    // Validate the message header
    if (!msg->IsValid())
    {
        DBG_INFO("Invalid message received from client index %lu.", index);
        pThis->SendInvalid(index);
        return;
    }

    unsigned short srcId = msg->header.SrcId;
    unsigned short dstId = msg->header.DstId;
    unsigned short type = msg->header.Type;

    if (dstId == IPC_CONTROL)
    {
        if (type == IPC_MSG_REGISTER)  // Client register
        {
            // Update client information
            pThis->ClientUpdate(index, srcId);
        }
        else if (type >= IPC_MSG_USER_MIN)  // Register client to user-defined msg type
        {
            pThis->MessageRegister(index, type);
        }
        else
        {
            // Reserved type 1~9, clients should not send these as control messages
            DBG_WARN("WARN: Client index %lu sent reserved control type 0x%04X, ignored.", index, type);
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
        int index_dst = -1;
        
        {
            std::lock_guard<std::mutex> lock(pThis->mMutex);
            for (const auto& client : pThis->mClients)
            {
                if (client.ClientId == dstId)
                {
                    index_dst = client.ClientIndex;
                    break;
                }
            }
        }
        
        if (index_dst == -1)
        {
            DBG_INFO("No client with ID 0x%04X found.", dstId);
            pThis->SendError(index, srcId);
            return;
        }

        pThis->Send(index_dst, msg);
        // Send ACK to the sender
        pThis->SendAck(index, srcId, dstId);
    }
}

void IPCServerBroker::RunBroker(const char* serverName, PIPC_BROKER_ON_CLIENT_CONNECT onConnect)
{
    m_pOnConnect = onConnect;

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

    if (!server->Listen(serverName, OnServerMessage, OnServerConnect, OnServerDisconnect, this))
    {
        delete server;
        server = nullptr;
        throw std::runtime_error("Failed to start IPC server");
    }

    server->Start();
}

void IPCServerBroker::RunBrokerAsync(const char* serverName, PIPC_BROKER_ON_CLIENT_CONNECT onConnect)
{
    std::string name(serverName);
    m_brokerThread = std::thread([this, name, onConnect]() {
        RunBroker(name.c_str(), onConnect);
    });
}

void IPCServerBroker::StopBroker()
{
    if (server)
    {
        server->Stop();

        // Wait for broker thread to exit before deleting server
        if (m_brokerThread.joinable())
        {
            m_brokerThread.join();
        }

        delete server;
        server = nullptr;
    }
}

void IPCServerBroker::SetOnClientDisconnect(PIPC_BROKER_ON_CLIENT_DISCONNECT onDisconnect)
{
    m_pOnClientDisconnect = onDisconnect;
}
