#pragma once

enum IPCMessageType : unsigned short
{
    IPC_MESSAGE_INVALID = 0x0000, // Invalid message type
    IPC_MESSAGE_MIN = 0x0001, // Base message type for IPC messages

    SESSION_LOCK = 0xA011, // Session lock event
    SESSION_UNLOCK = 0xA012, // Session unlock event
    SESSION_LOGON = 0xA013, // Session logon event
    SESSION_LOGOFF = 0xA014, // Session logoff event

    IPC_MESSAGE_MAX = 0xFFFF // Broadcast to all clients
};