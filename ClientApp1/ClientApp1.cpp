// ClientApp1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "..\IPC\IPC.h"
#include "..\IPC\IPCMessage.h"

#define IPC_BROKER_PIPE "\\\\.\\pipe\\MessageBroker_IPCBroker"

void OnClientMessage(void* msg, size_t buf_size)
{
    IpcMessage* pMsg = (IpcMessage*)msg;

    //dump message
    printf("Message SrcId: 0x%04X\n", pMsg->header.SrcId);
    printf("Message DstId: 0x%04X\n", pMsg->header.DstId);
    printf("Message Data Size: %d\n", pMsg->header.Size);
}

int main()
{
    void* pIpcClient = ipc_client_start(IPC_BROKER_PIPE, 0xF001, OnClientMessage);

    if (pIpcClient)
    {
        printf("Connect success\n");
    }
    else
    {
        printf("Connect fail\n");
        system("PAUSE");
        return -1;
    }

    system("PAUSE");

    int ret = ipc_client_send(pIpcClient, 0xF001, 0xF002, 1001, NULL, 0);
    if (ret == 0)
    {
        printf("Send message success\n");
    }
    else
    {
        printf("Send message fail: %d\n", ret);
    }

    system("PAUSE");

    ipc_client_stop(pIpcClient);
}