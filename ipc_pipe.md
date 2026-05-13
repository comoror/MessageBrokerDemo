# IPC Named Pipe 进程间通信库

## 1. 架构概述

本库基于 Windows Named Pipe 实现进程间通信，采用 **双层 API** 设计：

```
┌───────────────────────────────────────────────────┐
│             应用层 (Application)                    │
├─────────────────────┬─────────────────────────────┤
│   Broker API        │      Raw Pipe API           │
│   (消息路由协议)     │      (原始字节流)            │
├─────────────────────┴─────────────────────────────┤
│          CNamedPipeServer / CNamedPipeClient       │
│          (Overlapped I/O, Message Mode)            │
├───────────────────────────────────────────────────┤
│              Windows Named Pipe Kernel              │
└───────────────────────────────────────────────────┘
```

### 设计特点

| 特性 | 说明 |
|------|------|
| 传输层 | Windows Named Pipe (PIPE_TYPE_MESSAGE) |
| I/O 模型 | Overlapped I/O (异步非阻塞) |
| 最大连接数 | 16 个并发客户端 |
| 单消息上限 | 8192 字节 (含协议头) |
| 安全策略 | DACL 限制 Authenticated Users + 应用层签名认证 |
| 线程安全 | 写操作互斥锁保护，读操作独立线程 |
| 资源管理 | 无句柄泄漏，经自动化测试验证 |

---

## 2. API 分层说明

### 2.1 Raw Pipe API (原始管道)

直接使用 Named Pipe 传输原始字节数据，**无协议头、无路由**。适用于：
- 简单的点对点通信
- 自定义协议场景
- 不需要 Broker 中转的场景

### 2.2 Broker API (消息路由)

基于 IPC 协议的完整消息系统，Broker 负责消息中转和路由。支持：
- **单播** — 按 `dst_id` 精确投递到目标客户端
- **广播** — 按 `msg_type` 注册机制投递给所有订阅者
- **认证** — 连接时文件签名验证 + 客户端 ID 鉴权
- **踢出** — 重复 `client_id` 自动踢掉旧连接

---

## 3. 使用说明

### 3.1 Raw Pipe API 使用

**头文件**: `#include "IPC.h"`

#### Server 端

```c
// 回调定义
void OnMessage(void* ctx, unsigned long pipeIndex, void* data, size_t size) {
    // 处理收到的数据
}

void OnConnect(void* ctx, unsigned long pipeIndex) {
    printf("Client connected: pipe %lu\n", pipeIndex);
}

void OnDisconnect(void* ctx, unsigned long pipeIndex) {
    printf("Client disconnected: pipe %lu\n", pipeIndex);
}

// 启动 Server
IPC_PIPE_SERVER_HANDLE hServer = ipc_pipe_server_start(
    "\\\\.\\pipe\\MyPipe",
    OnMessage, OnConnect, OnDisconnect,
    NULL  // 用户上下文
);

// 向指定客户端发送
ipc_pipe_server_send(hServer, pipeIndex, data, dataSize);

// 广播给所有已连接客户端
ipc_pipe_server_broadcast(hServer, data, dataSize);

// 主动断开客户端
ipc_pipe_server_disconnect_client(hServer, pipeIndex);

// 停止 Server
ipc_pipe_server_stop(hServer);
```

#### Client 端

```c
void OnMessage(void* data, size_t size) {
    // 处理收到的数据
}

void OnConnect() { printf("Connected\n"); }
void OnDisconnect() { printf("Disconnected\n"); }

// 连接
IPC_PIPE_CLIENT_HANDLE hClient = ipc_pipe_client_connect(
    "\\\\.\\pipe\\MyPipe",
    OnMessage, OnConnect, OnDisconnect
);

// 发送数据
ipc_pipe_client_send(hClient, data, dataSize);

// 断开
ipc_pipe_client_disconnect(hClient);
```

### 3.2 Broker API 使用

#### Broker 端 (消息路由器)

```c
// 连接认证回调：验证文件签名
bool OnClientConnect(void* hPipe) {
    return VerifyPeerSignature(hPipe);
}

// 客户端 ID 鉴权
bool OnClientAuth(void* hPipe, unsigned short clientId) {
    return IsAuthorizedClient(clientId);
}

// 客户端断开通知
void OnClientDisconnect(unsigned short clientId) {
    printf("Client 0x%04X disconnected\n", clientId);
}

// 启动 Broker
IPC_BROKER_HANDLE hBroker = ipc_broker_start(
    "\\\\.\\pipe\\MyBroker",
    OnClientConnect, OnClientAuth, OnClientDisconnect
);

// 停止 Broker (所有客户端会收到断开通知)
ipc_broker_stop(hBroker);
```

#### Client 端 (消息收发)

```c
void OnMessage(void* inBuf, size_t bufSize) {
    // inBuf 为完整的 IpcMessage 结构体
    // 解析 msg_type, src_id, payload 等
}

// 连接 Broker，指定自身 client_id
IPC_CLIENT_HANDLE hClient = ipc_client_start(
    "\\\\.\\pipe\\MyBroker",
    0x0100,        // client_id
    OnMessage, OnConnect, OnDisconnect
);

// 单播：发送给 client_id = 0x0200 的客户端
ipc_client_send(hClient, 0x0200, MSG_TYPE_DATA, payload, payloadLen);

// 注册广播消息类型 (只有注册的客户端才会收到该类型的广播)
ipc_client_register_msg(hClient, MSG_TYPE_NOTIFY);

// 广播
ipc_client_broadcast(hClient, MSG_TYPE_NOTIFY, payload, payloadLen);

// 断开
ipc_client_stop(hClient);
```

### 3.3 IPC 协议消息结构

```
┌──────────────────────────────────────────────────┐
│  ipc_sig (4B)  │  msg_id  │  src_id  │  dst_id  │
│    'IPCM'      │ timestamp│  发送方  │  目标方   │
├──────────────────────────────────────────────────┤
│  msg_type (2B)  │  msg_len (2B)  │  payload ...  │
└──────────────────────────────────────────────────┘
```

| 字段 | 说明 |
|------|------|
| `dst_id = 0x0000` | 控制消息 (系统内部) |
| `dst_id = 0xFFFF` | 广播消息 |
| `dst_id = other` | 单播消息 |
| `msg_type 0~9` | 系统保留 (register, dst_not_found, ACK, invalid_header, msg_too_large, kick) |
| `msg_type 10~0xFFFF` | 用户自定义 |

### 3.4 错误码

| 错误码 | 值 | 含义 |
|--------|-----|------|
| `IPC_OK` | 0 | 成功 |
| `IPC_ERR_INVALID_PARAM` | -1 | 参数无效 (空指针等) |
| `IPC_ERR_NOT_CONNECTED` | -2 | 未连接 |
| `IPC_ERR_SEND_FAILED` | -3 | 发送失败 |
| `IPC_ERR_DATA_TOO_LARGE` | -4 | 数据超过最大载荷 |
| `IPC_ERR_RETRY_FAILED` | -5 | 重试失败 |

---

## 4. 性能测试结果

测试环境：单机本地通信，Debug 构建，x64

### 4.1 吞吐量测试 (Broker 单播)

每轮发送 10,000 条消息，测量端到端投递速率：

| 载荷大小 | 吞吐量 (msg/sec) | 平均延迟 (us) |
|----------|------------------|---------------|
| 64 B | 22,100 ~ 23,900 | 41.8 ~ 45.2 |
| 512 B | 19,400 ~ 23,500 | 42.6 ~ 51.4 |
| 4 KB | 21,000 ~ 22,400 | 44.6 ~ 47.5 |
| 8 KB | 18,100 ~ 21,500 | 46.5 ~ 55.1 |

### 4.2 广播吞吐量

1,000 条广播 x 4 个接收客户端 = 4,000 次投递：

| 场景 | 吞吐量 (msg/sec) | 平均延迟 (us) |
|------|------------------|---------------|
| 广播 64B (4 receivers) | 32,700 ~ 37,100 | 26.9 ~ 30.6 |

### 4.3 多发送者并发

4 个线程同时发送，测量总吞吐：

| 场景 | 吞吐量 (msg/sec) | 平均延迟 (us) |
|------|------------------|---------------|
| 4 senders x 2500 msg | 20,300 ~ 24,400 | 41.0 ~ 49.1 |

### 4.4 往返延迟 (Roundtrip Latency)

500 次 Client A → Broker → Client B → Broker → Client A：

| 指标 | 值 |
|------|-----|
| 最小延迟 | 188 ~ 338 us |
| 平均延迟 | 7,700 ~ 9,800 us |
| P50 | 8,300 ~ 10,500 us |
| P99 | 11,600 ~ 14,600 us |

> 注：Debug 模式下延迟偏高，Release 构建可期望显著降低。

---

## 5. 可靠性测试

### 5.1 句柄泄漏检测

通过 `GetProcessHandleCount()` 在启动/停止前后测量 HANDLE 数量：

| 测试项目 | 循环次数 | 泄漏句柄 |
|---------|---------|---------|
| Raw Pipe Server 启动/停止 | 10 次 | 0 |
| Raw Pipe Client 连接/断开 | 10 次 | 0 |
| Broker 启动/停止 | 10 次 | 0 |
| Broker Client 连接/断开 | 10 次 | 0 |

### 5.2 压力测试

| 测试场景 | 结果 |
|---------|------|
| 最大并发客户端 (15 个) | 通过 |
| 8 线程并发发送 (各 100 条) | 通过，无丢失 |
| 快速重连 (4 客户端 x 10 次) | 通过 |
| 广播风暴 (500 条 x 8 接收者) | 通过 |
| 混合负载 (2 秒持续读写) | 通过，~500 msg/2s |

### 5.3 异常处理

| 测试场景 | 预期行为 | 结果 |
|---------|---------|------|
| 空指针句柄发送 | 返回 IPC_ERR_INVALID_PARAM | 通过 |
| 超大载荷发送 | 返回 IPC_ERR_DATA_TOO_LARGE | 通过 |
| 停止后发送 | 返回 IPC_ERR_INVALID_PARAM | 通过 |
| 无效 pipe index | 返回错误，不崩溃 | 通过 |
| Broker 停止 (null) | 不崩溃 | 通过 |
| Client 停止 (null) | 不崩溃 | 通过 |

---

## 6. 安全机制

| 层级 | 措施 |
|------|------|
| 管道权限 | DACL 限定 Authenticated Users (`D:(A;;GA;;;AU)`)，拒绝匿名/Guest 访问 |
| 连接认证 | `OnClientConnect` 回调中执行文件签名验证 |
| 客户端鉴权 | `OnClientAuth` 回调按 client_id 决定是否放行 |
| 协议校验 | 消息头签名 'IPCM' 验证，防止非法数据注入 |
| 重复 ID 保护 | 新连接踢掉旧同名客户端，防止冒用 |

---

## 7. 自动化测试概览

测试程序 `IPCTest.exe` 包含 6 个测试套件，50 个测试用例：

| Suite | 用例数 | 覆盖范围 |
|-------|--------|---------|
| RawPipe | 11 | 原始管道 API 功能 + 句柄泄漏 |
| BrokerLifecycle | 9 | Broker/Client 生命周期 + 句柄泄漏 |
| BrokerFunctional | 10 | 单播/广播/注册/踢出/载荷完整性 |
| BrokerPerformance | 7 | 吞吐量/延迟/多发送者 |
| BrokerStress | 5 | 最大连接/并发/重连/风暴 |
| ErrorHandling | 8 | 空指针/越界/超大/异常场景 |

运行方式：
```
IPCTest.exe              # 运行全部测试 (~25秒)
IPCTest.exe RawPipe      # 只运行 Raw Pipe 套件
IPCTest.exe BrokerPerformance  # 只运行性能测试
```

---

## 8. 两种 API 对比

| 对比项 | Raw Pipe API | Broker API |
|--------|-------------|------------|
| 协议开销 | 无 | IPC 消息头 (~16 字节) |
| 路由 | 无 (点对点) | 支持单播/广播 |
| 认证 | 无 (需自行实现) | 内建签名 + ID 认证 |
| 消息类型 | 无 | 系统保留 + 用户自定义 |
| 客户端标识 | pipeIndex (连接序号) | client_id (逻辑 ID) |
| 适用场景 | 简单通信、自定义协议 | 多进程协作、消息总线 |
| 连接管理 | 手动 | Broker 自动管理 |

---

## 9. 编译与集成

- **开发环境**: Visual Studio 2022, C++17, x64
- **输出**: IPC.lib (静态库)
- **头文件**: 仅需包含 `IPC.h`
- **依赖**: Windows SDK (Named Pipe, Overlapped I/O)
- **集成方式**: 链接 IPC.lib，包含 IPC.h 即可使用两种 API
