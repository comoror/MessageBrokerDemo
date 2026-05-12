#pragma once

#include <windows.h>
#include <memory>
#include <stdexcept>

//Destination IDs
const unsigned short IPC_CONTROL   = 0;        // Control Message  
const unsigned short IPC_BROADCAST = 0xFFFF;   // Broadcast to all clients

// IPC internal message types (spec-defined, 0~9 reserved)
constexpr unsigned short IPC_MSG_REGISTER      = 0;   // Client register
constexpr unsigned short IPC_MSG_DST_NOT_FOUND = 1;   // Destination not found
constexpr unsigned short IPC_MSG_ACK           = 2;   // Acknowledgment
constexpr unsigned short IPC_MSG_INVALID       = 3;   // Invalid message (header verify failed)
constexpr unsigned short IPC_MSG_TOO_LARGE     = 4;   // Message size too large
constexpr unsigned short IPC_MSG_HEARTBEAT     = 5;   // Heartbeat
constexpr unsigned short IPC_MSG_KICK          = 6;   // Client replaced by another with same id
constexpr unsigned short IPC_MSG_USER_MIN      = 10;  // User-defined message types start here

struct IPCHeader
{
    unsigned long   Signature;   // Signature for IPC messages
    unsigned long   Timestamp;   // Timestamp of the message
    unsigned short  SrcId;      // Source ID
    unsigned short  DstId;      // Destination ID
    unsigned short  Type;       // Message type
    unsigned short  Size;       // Size of the total message

    IPCHeader()
        : Signature('IPCM'),
        Timestamp(0),
        SrcId(0),
        DstId(0),
        Type(0),
        Size(0)
    {
    }
};

struct IpcMessage
{
    static constexpr unsigned short MAX_PAYLOAD_SIZE = 8192 - sizeof(IPCHeader);

    IPCHeader header;
    unsigned char Data[MAX_PAYLOAD_SIZE];

    IpcMessage(unsigned short srcId, unsigned short dstId, unsigned short msgType, void* data, unsigned short dataSize)
    {
        memset(Data, 0, sizeof(Data)); // Clear the data buffer

        header.Timestamp = GetTickCount(); // Set the timestamp as msg_id
        header.SrcId = srcId; // Default source ID
        header.DstId = dstId; // Default destination ID
        header.Type = msgType; // Default message type

        header.Size = dataSize + sizeof(IPCHeader); // Set the size of the data
        if (dataSize > 0 && data != nullptr)
        {
            if (dataSize > sizeof(Data))
            {
                // Handle error: data size exceeds buffer size
                throw std::runtime_error("Data size exceeds buffer size");
            }
            memcpy(Data, data, dataSize); // Copy the data into the message
        }
    }

    bool IsValid()
    {
        if (header.Signature != 'IPCM')
        {
            return false;
        }
        if (header.Size < sizeof(IPCHeader) || header.Size > sizeof(IpcMessage))
        {
            return false;
        }
        return true;
    }
};

typedef std::shared_ptr<IpcMessage> IpcMessagePtr;
