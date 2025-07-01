#pragma once

struct IPCHeader
{
    unsigned long   Signature;   // Signature for IPC messages
    unsigned long   Timestamp;   // Timestamp of the message
    unsigned short  SrcId;      // Source ID
    unsigned short  DstId;      // Destination ID
    unsigned short  Type;       // Message type
    unsigned short  DataSize;   // Size of the data

    IPCHeader()
        : Signature('IPCM'),
        Timestamp(0),
        SrcId(0),
        DstId(0),
        Type(0),
        DataSize(0)
    {
    }
};

struct IpcMessage
{
    IPCHeader header;
    char Data[1];
};

//Destination IDs
const unsigned short IPC_DST_BROADCAST          = 0xFFFF;   // Broadcast to all clients
const unsigned short IPC_DST_REGISTER_CLIENT    = 0x0000;   // Register client to a specific message type
const unsigned short IPC_DST_REGISTER_MESSAGE   = 0x0001;   // Register message type

