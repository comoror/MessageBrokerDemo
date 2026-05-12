// ClientApp1.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "..\IPC\IPC.h"
#include "..\IPC\IPCMessage.h"

#define IPC_BROKER_PIPE "\\\\.\\pipe\\MessageBroker_IPCBroker"

#define IPC_CLIENT_TEST     0xF001
#define IPC_CLIENT_PERF     5
#define IPC_CLIENT_HANG     3

void OnClientMessage(void* msg, size_t buf_size)
{
    IpcMessage* pMsg = (IpcMessage*)msg;

    //dump message
    printf("Message SrcId: 0x%04X\n", pMsg->header.SrcId);
    printf("Message DstId: 0x%04X\n", pMsg->header.DstId);
    printf("Message Data Size: %d\n", pMsg->header.Size);

    if (pMsg->header.SrcId == 0)
    {
        if (pMsg->header.Type == IPC_ERROR::IPC_ERROR_DST_NOT_ONLINE)
        {
            printf("0x%04X not on line\n", pMsg->header.DstId);
        }
        else
        {
            printf("error type: %d\n", pMsg->header.Type);
        }
    }
}

int main()
{
    void* pIpcClient = ipc_client_start(IPC_BROKER_PIPE, IPC_CLIENT_TEST, OnClientMessage);

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

    int ret = ipc_client_send(pIpcClient, IPC_CLIENT_TEST, IPC_CLIENT_PERF, 1000, 
        (void*)"123123", sizeof("123123"));
    if (ret == 0)
    {
        printf("Send message success\n");
    }
    else
    {
        printf("Send message fail: %d\n", ret);
    }

    system("PAUSE");

    std::string strJson = "{\"pid\":234, \"pname\" : \"example.exe\" }";

    ret = ipc_client_send(pIpcClient, IPC_CLIENT_TEST, IPC_CLIENT_HANG, 1000, 
        (void*)strJson.c_str(), strJson.length());
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