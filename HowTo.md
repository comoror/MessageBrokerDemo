# IPC 库使用指南

## 概述

本 IPC 库基于 Windows 命名管道实现进程间通信，采用 Broker（消息代理）架构。Broker 作为中心转发节点，所有客户端连接到 Broker，由 Broker 负责消息路由（单播/广播）。

```
  ClientA ──┐                ┌── ClientC
             ├── Broker ────┤
  ClientB ──┘                └── ClientD
```

## 头文件

```cpp
#include "IPC.h"          // C API 接口
#include "IPCMessage.h"   // 消息结构体和常量定义
```

## 第一部分：Broker（消息代理）

### 启动 Broker

```cpp
#define MY_PIPE "\\\\.\\pipe\\MyApp_Broker"

// 认证回调：返回 true 允许连接，返回 false 拒绝连接
bool OnAuth(void* hPipe)
{
    // hPipe 是客户端的命名管道 HANDLE
    // 可通过 PID 验证客户端进程的文件签名：
    // ULONG pid = 0;
    // GetNamedPipeClientProcessId((HANDLE)hPipe, &pid);
    // return VerifyProcessSignature(pid);
    return true;
}

// 可选：客户端断开连接时的回调
void OnClientDisconnect(unsigned short clientId)
{
    printf("客户端 0x%04X 已断开\n", clientId);
}

// 启动 Broker（非阻塞，在后台线程运行）
IPC_BROKER_HANDLE hBroker = ipc_broker_start(MY_PIPE, OnAuth, OnClientDisconnect);
if (!hBroker)
{
    printf("Broker 启动失败\n");
    return -1;
}
```

### 停止 Broker

```cpp
ipc_broker_stop(hBroker);  // 停止服务、等待后台线程结束、释放资源
```

### Broker 行为说明

- Broker 自动处理客户端注册（`msg_type = 0`），无需手动干预。
- **单播**：将消息路由到指定 `dst_id` 的客户端。若目标客户端未连接，Broker 会向发送方回复 `IPC_MSG_DST_NOT_FOUND`（type 1）。
- **广播**（`dst_id = 0xFFFF`）：仅投递给通过 `ipc_client_register_msg` 注册了该 `msg_type` 的客户端。
- **重复 client_id**：若新客户端使用已有的 `client_id` 连接，旧连接将被踢下线（收到 `IPC_MSG_KICK`，type 6）。

---

## 第二部分：Client（客户端）

### 连接到 Broker

```cpp
#define BROKER_PIPE "\\\\.\\pipe\\MyApp_Broker"
#define MY_CLIENT_ID  0xF001  // 本客户端的唯一 ID（范围：1 ~ 0xFFFE）

void OnMessage(void* msg, size_t bufSize)
{
    IpcMessage* pMsg = (IpcMessage*)msg;
    
    // 处理系统消息（type 0~9）
    if (pMsg->header.Type < IPC_MSG_USER_MIN)
    {
        switch (pMsg->header.Type)
        {
        case IPC_MSG_DST_NOT_FOUND:
            printf("目标 0x%04X 不在线\n", pMsg->header.DstId);
            break;
        case IPC_MSG_KICK:
            printf("被踢下线：另一个客户端使用了相同 ID\n");
            break;
        default:
            break;
        }
        return;
    }
    
    // 处理用户自定义消息（type >= 10）
    printf("收到消息 msg_type=%d, 来自 0x%04X, 载荷大小=%d\n",
        pMsg->header.Type, pMsg->header.SrcId,
        pMsg->header.Size - sizeof(IPCHeader));
    
    // 访问载荷数据：
    // pMsg->Data 包含载荷字节
    // 载荷长度 = pMsg->header.Size - sizeof(IPCHeader)
}

void OnConnect()
{
    printf("已连接到 Broker\n");
}

void OnDisconnect()
{
    printf("与 Broker 断开连接\n");
}

// 连接到 Broker
IPC_CLIENT_HANDLE hClient = ipc_client_start(
    BROKER_PIPE,
    MY_CLIENT_ID,
    OnMessage,
    OnConnect,      // 可选，可传 nullptr
    OnDisconnect    // 可选，可传 nullptr
);

if (!hClient)
{
    printf("连接失败\n");
    return -1;
}
```

### 发送单播消息

向指定 `client_id` 的客户端发送消息：

```cpp
// 发送到客户端 0xF002，用户自定义 msg_type 1000
const char* payload = "Hello";
IPC_RESULT ret = ipc_client_send(hClient, 0xF002, 1000, (void*)payload, strlen(payload));
if (ret != IPC_OK)
{
    printf("发送失败: %d\n", ret);
}
```

若目标客户端未连接，Broker 会回复 `IPC_MSG_DST_NOT_FOUND`。

### 发送广播消息

向所有注册了该 `msg_type` 的客户端广播消息：

```cpp
IPC_RESULT ret = ipc_client_broadcast(hClient, 1000, (void*)"event", 5);
```

### 注册广播消息类型

客户端必须显式注册感兴趣的 `msg_type`，才能接收对应类型的广播消息：

```cpp
// 注册接收 msg_type 1000 的广播
IPC_RESULT ret = ipc_client_register_msg(hClient, 1000);
```

可多次调用以注册多个消息类型。

### 断开连接

```cpp
ipc_client_stop(hClient);  // 断开连接并释放资源
```

---

## 第三部分：消息结构

### 消息头字段

| 字段        | 类型             | 说明                                                |
|-------------|------------------|-----------------------------------------------------|
| `Signature` | `unsigned long`  | 固定为 `'IPCM'`，由 Broker 校验                     |
| `MsgId`     | `unsigned long`  | 时间戳（`GetTickCount`），自动填充                   |
| `SrcId`     | `unsigned short` | 源客户端 ID（由库自动填充）                          |
| `DstId`     | `unsigned short` | 目标：`0` = 控制消息，`0xFFFF` = 广播，其他 = 单播   |
| `Type`      | `unsigned short` | 消息类型（0-9 保留，>= 10 用户自定义）               |
| `Size`      | `unsigned short` | 消息总大小（消息头 + 载荷）                          |

### 保留消息类型（0 ~ 9）

| 值  | 常量                    | 方向             | 含义                         |
|-----|-------------------------|------------------|------------------------------|
| 0   | `IPC_MSG_REGISTER`      | Client -> Broker | 客户端注册（自动发送）        |
| 1   | `IPC_MSG_DST_NOT_FOUND` | Broker -> Client | 单播目标未找到               |
| 2   | `IPC_MSG_ACK`           | Broker -> Client | 确认应答                     |
| 3   | `IPC_MSG_INVALID`       | Broker -> Client | 消息头校验失败               |
| 4   | `IPC_MSG_TOO_LARGE`     | Broker -> Client | 消息超过最大允许大小          |
| 5   | `IPC_MSG_HEARTBEAT`     | 双向             | 心跳                         |
| 6   | `IPC_MSG_KICK`          | Broker -> Client | 被同 ID 的新客户端替换踢下线  |

### 载荷限制

- 消息最大总大小：8192 字节
- 最大载荷：`8192 - sizeof(IPCHeader)` = 8176 字节

---

## 第四部分：错误码

```cpp
enum IPC_RESULT : int
{
    IPC_OK                 =  0,   // 成功
    IPC_ERR_INVALID_PARAM  = -1,   // 句柄为空或参数无效
    IPC_ERR_NOT_CONNECTED  = -2,   // 客户端未连接
    IPC_ERR_SEND_FAILED    = -3,   // 管道写入失败
    IPC_ERR_DATA_TOO_LARGE = -4,   // 载荷超过 MAX_PAYLOAD_SIZE
    IPC_ERR_RETRY_FAILED   = -5,   // 注册消息重试耗尽
};
```

---

## 第五部分：完整示例

### Broker 进程

```cpp
#include "IPC.h"

#define PIPE_NAME "\\\\.\\pipe\\MyApp_Broker"

bool OnAuth(void* hPipe) { return true; }

void OnDisconnect(unsigned short clientId)
{
    printf("客户端 0x%04X 已离开\n", clientId);
}

int main()
{
    IPC_BROKER_HANDLE hBroker = ipc_broker_start(PIPE_NAME, OnAuth, OnDisconnect);
    if (!hBroker) return -1;

    printf("Broker 运行中，按 Enter 停止。\n");
    getchar();

    ipc_broker_stop(hBroker);
    return 0;
}
```

### 客户端 A（发送方）

```cpp
#include "IPC.h"
#include "IPCMessage.h"

#define PIPE_NAME    "\\\\.\\pipe\\MyApp_Broker"
#define CLIENT_A_ID  0x0001
#define CLIENT_B_ID  0x0002
#define MSG_HELLO    1000    // 用户自定义类型

void OnMessage(void* msg, size_t bufSize)
{
    IpcMessage* pMsg = (IpcMessage*)msg;
    if (pMsg->header.Type == IPC_MSG_DST_NOT_FOUND)
        printf("目标不在线\n");
}

int main()
{
    IPC_CLIENT_HANDLE hClient = ipc_client_start(PIPE_NAME, CLIENT_A_ID, OnMessage, nullptr, nullptr);
    if (!hClient) return -1;

    // 单播发送给客户端 B
    const char* data = "Hello from A";
    ipc_client_send(hClient, CLIENT_B_ID, MSG_HELLO, (void*)data, strlen(data));

    // 广播（仅注册了 MSG_HELLO 的客户端能收到）
    ipc_client_broadcast(hClient, MSG_HELLO, (void*)"Hi all", 6);

    getchar();
    ipc_client_stop(hClient);
    return 0;
}
```

### 客户端 B（接收方）

```cpp
#include "IPC.h"
#include "IPCMessage.h"

#define PIPE_NAME    "\\\\.\\pipe\\MyApp_Broker"
#define CLIENT_B_ID  0x0002
#define MSG_HELLO    1000

void OnMessage(void* msg, size_t bufSize)
{
    IpcMessage* pMsg = (IpcMessage*)msg;
    if (pMsg->header.Type < IPC_MSG_USER_MIN) return;  // 跳过系统消息

    unsigned short payloadLen = pMsg->header.Size - sizeof(IPCHeader);
    printf("来自 0x%04X: %.*s\n", pMsg->header.SrcId, payloadLen, pMsg->Data);
}

int main()
{
    IPC_CLIENT_HANDLE hClient = ipc_client_start(PIPE_NAME, CLIENT_B_ID, OnMessage, nullptr, nullptr);
    if (!hClient) return -1;

    // 注册接收 MSG_HELLO 类型的广播
    ipc_client_register_msg(hClient, MSG_HELLO);

    printf("监听中，按 Enter 退出。\n");
    getchar();
    ipc_client_stop(hClient);
    return 0;
}
```

---

## 注意事项

- Broker 必须在客户端连接**之前**启动。客户端在所有管道实例繁忙时会自动重试等待（最多 20 秒）。
- `client_id` 在所有已连接客户端中必须唯一。有效范围：`0x0001` ~ `0xFFFE`。
- 用户自定义 `msg_type` 必须 >= `IPC_MSG_USER_MIN`（10）。
- `OnMessage` 回调在工作线程中被调用 — 若访问共享状态需自行保证线程安全。
- `OnAuth` 回调接收原始管道 `HANDLE`，可通过 `GetNamedPipeClientProcessId` 获取客户端 PID 进行签名验证。
