// ClientApp2.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "..\IPC\IPCClient.h"
#include "..\BrokerSvc\Common.h"

void OnClientMessage(void* msg)
{
    IpcMessage* pMsg = (IpcMessage*)msg;

    //dump message
    printf("Message SrcId: 0x%04X\n", pMsg->header.SrcId);
    printf("Message DstId: 0x%04X\n", pMsg->header.DstId);
    printf("Message Data Size: %d\n", pMsg->header.Size);

    DWORD dwSessionID = *(DWORD*)pMsg->Data;
    if (pMsg->header.Type == IPCMessageType::SESSION_LOCK)
    {
        printf("Session %d locked\n", dwSessionID);
    }
    else if (pMsg->header.Type == IPCMessageType::SESSION_UNLOCK)
    {
        printf("Session %d unlocked\n", dwSessionID);
    }
    else
    {
        printf("Unknown message type: 0x%04X\n", pMsg->header.Type);
    }
}

int main()
{
    IPCClient client(0xF111);

    if (client.Connect("\\\\.\\pipe\\DemoBroker", OnClientMessage))
    {
        printf("Connect success\n");
    }
    else
    {
        printf("Connect fail\n");
        system("PAUSE");
        return -1;
    }

    client.RegisterMessage(IPCMessageType::SESSION_UNLOCK);

    system("PAUSE");

    client.Disconnect();
}
