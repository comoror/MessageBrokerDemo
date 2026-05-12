# IPC 工程 Code Review (第二轮)

## 修复状态总览

| # | 问题 | 状态 |
|---|---|---|
| 1 | ReadPipe 字节丢失 | **未修复** |
| 2 | index_dst int/unsigned long | **未修复** |
| 3 | WaitForMultipleObjects 返回值未校验 | **已修复** |
| 4 | 客户端读线程 UAF | **部分修复** (新增 m_hEventThreadDone，但 detach 路径仍在) |
| 5 | 裸指针管理 | **未修复** |
| 6 | Release 下无日志 | **未修复** |
| 7 | StopBroker 竞态 | **未修复** |
| 8 | broker 启动时序 | **未修复** |
| 9 | memset 8KB 性能 | **未修复** |
| 10 | WritePipe Overlapped 用法 | **未修复** |
| 11 | GetTickCount() | **未修复** |
| 12 | WaitNamedPipe 硬编码超时 | **未修复** |
| 13 | IpcMessage 构造函数抛异常 | **未修复** |
| 14 | new(std::nothrow) 混用 | **已修复** (IPCClient.cpp 也改为 nothrow) |
| 15 | LPTSTR 非 const | **已修复** (改为 LPCTSTR) |
| 16 | 安全描述符 GA 权限过大 | **未修复** |
| 17 | spec.md 命名不一致 | **未修复** |

**新增变化:** 回调体系重构 — 将 `PIPC_BROKER_ON_AUTH` 拆分为 `PIPC_BROKER_ON_CLIENT_CONNECT` (连接时) 和 `PIPC_BROKER_ON_CLIENT_AUTH` (注册时)，新增 `SendAck` 移除。整体设计更清晰。

---

## 遗留严重问题 (High)

### H1. `ReadPipe()` 读取字节数丢失 — 客户端仍未修复

**文件:** `CNamedPipeClient.cpp:173-178`

```cpp
else
{
    //read completed
    DBG_INFO("Read successfully completed.\n");
    OnMessage(m_PipeReadBuffer, MAX_PIPE_BUFFER_SIZE);  // BUG: 应该用 dwRead
}
```

当 `ReadFile` 同步完成时，代码用 `MAX_PIPE_BUFFER_SIZE` (8192) 而非实际读取的 `dwRead` 作为消息大小。上层收到尾部含垃圾字节的数据，可能导致消息校验异常或数据损坏。

服务端 `CNamedPipeServer::ReadPipe()` 同样存在此问题（第 271 行），`mBytesRead` 虽被赋值但后续 `GetPendingOperationResult` 的 `else` 分支（第 226-229 行）在非 pending 状态下直接调用 `OnMessage`，使用的是上一次的 `mBytesRead` 而非当前同步读取的值。

---

### H2. `index_dst` 用 `int` 存 `unsigned long`

**文件:** `IPCServerBroker.cpp:377`

```cpp
int index_dst = -1;
```

类型不一致，`unsigned long` 赋值给 `int` 存在溢出风险。建议用 `std::optional<unsigned long>` 或 `DWORD_MAX` 作哨兵值。

---

## 遗留中等问题 (Medium)

### M1. 裸指针管理 — `IPCServer::pServer` 和 `IPCClient::pClient`

**文件:** `IPCServer.h:31`, `IPCClient.h:24`

`new`/`delete` 手动管理，异常路径可能泄漏。建议用 `std::unique_ptr`。注意 `IPCClient::Connect()` 已改为 `new(std::nothrow)` 并检查返回值（已修复），但 `IPCServer::Listen()` 中的 `delete pServer` 在异常场景下仍可能遗漏。

---

### M2. Release 下所有日志静默

**文件:** `pch.h:16-24`

Release 构建下 `DBG_ERROR` 也被编译掉。生产环境无法排障。建议至少保留 `DBG_ERROR`/`DBG_WARN`，或引入 spdlog（项目已有此依赖）。

---

### M3. `IPCServerBroker::StopBroker()` 无锁保护

**文件:** `IPCServerBroker.cpp:440-455`

```cpp
void IPCServerBroker::StopBroker()
{
    if (server)  // <-- 无锁，多线程/析构+外部调用可能 double-free
    {
        ...
    }
}
```

---

### M4. `ipc_broker_start` 返回时 broker 可能尚未就绪

**文件:** `IPC.cpp:132`

`RunBrokerAsync` 异步启动后立即返回 handle，调用方可能在 `Listen()` 完成前连接。虽然客户端有 `WaitNamedPipe` 重试，但这是隐式时序依赖。

---

### M5. `IpcMessage` 构造函数中 `memset` 整个 8KB buffer

**文件:** `IPCMessage.h:49`

```cpp
memset(Data, 0, sizeof(Data)); // 每次构造清零 ~8KB
```

高频场景下不必要的性能开销。

---

### M6. `CNamedPipeServer::WritePipe()` Overlapped 用法

**文件:** `CNamedPipeServer.cpp:307`

`ovWrite.hEvent` 未设置（为 `NULL`），依赖文件句柄默认行为。当 `WriteFile` 同步返回 `TRUE` 时，`bytesWritten` 可能为 0（overlapped 操作的合法返回），代码却认为写入成功。

---

## 遗留低级问题 (Low)

### L1. `GetTickCount()` 作为消息 ID

**文件:** `IPCMessage.h:51` — ~15ms 精度，49.7 天回绕。建议用 `QueryPerformanceCounter` 或原子计数器。

### L2. `WaitNamedPipe` 硬编码 20 秒超时

**文件:** `CNamedPipeClient.cpp:96` — 建议参数化。

### L3. `IpcMessage` 构造函数抛异常

**文件:** `IPCMessage.h:62` — IPC 框架中抛异常不合适，建议返回错误码。

### L4. 安全描述符 GA 权限过大

**文件:** `CNamedPipeServer.cpp:82` — `GA` (Generic All) 可收紧为 `GR` + `GW`。

### L5. `ipc_broker_start` 命名歧义

**文件:** `IPC.h:57` — 函数名暗示同步，实际内部调用 `RunBrokerAsync`。

---

## 新增问题

### M7. `CNamedPipeClient::Disconnect()` 的 `detach` 路径仍存在

**文件:** `CNamedPipeClient.cpp:218-220`

```cpp
else
{
    // 在工作线程自身调用 Disconnect：不能 join 自己，直接返回，线程会自行退出。
    m_thread.detach();
}
```

虽然新增了 `m_hEventThreadDone` 事件和析构函数中的 `WaitForSingleObject`（5秒超时），但 `detach` 后 `std::thread` 对象不再关联线程，后续再次调用 `Disconnect()` 时 `m_thread.joinable()` 返回 `false`，不会再等待。如果析构函数在 5 秒内未被调用（或 `m_hEventThreadDone` 被意外关闭），仍可能 UAF。

建议：用 `m_hEventThreadDone` 完全替代 `detach` 逻辑 — 在 `Disconnect()` 中无论是否在工作线程内，都只 `SetEvent(m_hEventExit)` 然后返回，析构函数中统一 `WaitForSingleObject(m_hEventThreadDone)`。移除 `m_thread.detach()` 和 `m_thread.join()` 调用。

---

### H3. `CNamedPipeServer::ReadPipe()` 同步完成路径的 `mBytesRead` 传递问题

**文件:** `CNamedPipeServer.cpp:271-276`

```cpp
if (success && m_instPipes[pipeIndex].mBytesRead != 0)
{
    m_instPipes[pipeIndex].mPendingIO = FALSE;
    return 0;  // 直接返回，没有调用 OnMessage
}
```

`ReadFile` 同步完成时，`mBytesRead` 被正确设置，但函数直接 `return 0` 而未调用 `OnMessage`。消息处理依赖下一次 `GetPendingOperationResult` 中 `!mPendingIO` 的 `else` 分支，但此时 `mCurrentState` 仍为 `READING`，会直接调用 `OnMessage` — 这条路径虽然最终能工作，但依赖了隐式的状态流转，且如果 `ReadFile` 同步读到 0 字节（连接关闭），`mBytesRead != 0` 检查会跳过，不会触发断连重连。

---

## 总结

| 级别 | 数量 | 关键项 |
|---|---|---|
| **High** | 3 | ReadPipe 字节丢失 (H1)、index_dst 类型 (H2)、ReadPipe 同步路径 (H3) |
| **Medium** | 7 | 裸指针 (M1)、Release 无日志 (M2)、StopBroker 竞态 (M3)、broker 时序 (M4)、memset (M5)、Overlapped (M6)、detach 路径 (M7) |
| **Low** | 5 | GetTickCount (L1)、硬编码超时 (L2)、异常使用 (L3)、安全描述符 (L4)、命名 (L5) |

**建议优先修复顺序:**
1. **H1 + H3** — ReadPipe 字节丢失，直接影响消息正确性
2. **M7** — detach 路径的线程安全
3. **M3** — StopBroker 竞态
