#pragma once

#include <memory>
#include <iostream>

//Destination IDs
const unsigned short IPC_CONTROL   = 0;        // Control Message  
const unsigned short IPC_BROADCAST = 0xFFFF;   // Broadcast to all clients

enum IPC_ERROR : unsigned short
{
    IPC_ERROR_INVALID = 0,          // Invalid message
    IPC_ERROR_DST_NOT_ONLINE = 1,   // Destination client not connected
    IPC_ERROR_MAX = 100             // Maximum error code
};

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
    IPCHeader header;
    unsigned char Data[2 * 4096 - sizeof(IPCHeader)];

    IpcMessage(unsigned short srcId, unsigned short dstId, unsigned short msgType, void* data, unsigned short dataSize)
    {
        memset(Data, 0, sizeof(Data)); // Clear the data buffer

        //header.Timestamp = GetTickCount(); // Set the timestamp to current tick count
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
        // Check if the message is valid
        if (header.Signature != 'IPCM') // Check signature
        {
            std::cerr << "Invalid IPC message signature: " << header.Signature << std::endl;
            return false;
        }
        if (header.Size < sizeof(IPCHeader) || header.Size > sizeof(IpcMessage))
        {
            std::cerr << "Invalid IPC message size: " << header.Size << std::endl;
            return false;
        }
        return true;
    }

    //~IpcMessage()
    //{
    //    if (Data)
    //    {
    //        delete[] Data; // Free the allocated memory for data
    //        Data = nullptr;
    //    }
    //}
};

typedef std::shared_ptr<IpcMessage> IpcMessagePtr;
