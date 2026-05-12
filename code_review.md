# IPC 工程 Code Review

## 严重问题 (High)

### 1. `ReadPipe()` 读取字节数丢失 — 客户端和服务端都存在

**文件:** `CNamedPipeClient.cpp:155-158`

当 `ReadFile` 同步完成时，`m_bPendingIO == FALSE`，但代码用 `MAX_PIPE_BUFFER_SIZE` 作为消息大小传给回调，而非实际读取的字节数 `dwRead`：

```cpp
else
{
    //read completed
    DBG_INFO("Read successfully completed.\n");
    OnMessage(m_PipeReadBuffer, MAX_PIPE_BUFFER_SIZE);  // BUG: 应该用 dwRead
}
```

这会导致上层收到的数据尾部包含垃圾字节，`IpcMessage::IsValid()` 中的 `header.Size` 校验可能侥幸通过（因为尾部是脏数据），也可能因 Size 不匹配而误判为非法消息。

---

### 2. `index_dst` 用 `int` 存 `unsigned long`，比较 `== -1` 有隐患

**文件:** `IPCServerBroker.cpp:374-388`

```cpp
int index_dst = -1;
// ... search loop ...
if (index_dst == -1) { ... }
```

`ClientIndex` 是 `unsigned long`，赋值给 `int` 后如果 pipe index 值大于 `INT_MAX` 会溢出。虽然实际场景中 pipe index 不会超过 16，但类型不一致是潜在风险。建议用 `std::optional<unsigned long>` 或初始化为特殊值。

---

### 3. `CNamedPipeServer::Run()` 未校验 `WaitForMultipleObjects` 返回值

**文件:** `CNamedPipeServer.cpp:57-68`

```cpp
DWORD i = dwWaitResult - WAIT_OBJECT_0;
if (i == nMaxPipes) // exit event
{
    break;
}
```

当 `WaitForMultipleObjects` 返回 `WAIT_FAILED` 时，`dwWaitResult - WAIT_OBJECT_0` 的结果是未定义行为。同样 `WAIT_TIMEOUT` 和 `WAIT_ABANDONED_0 + n` 也没有处理。循环会用随机索引访问数组，导致越界。

---

### 4. 客户端读线程中的 `this` 捕获 — 潜在 UAF

**文件:** `CNamedPipeClient.cpp:111`

```cpp
m_thread = std::thread([=]() {  // captures `this`
    // ... uses m_hEventExit, m_ovRead, m_hPipe, m_bPendingIO, m_PipeReadBuffer ...
});
```

lambda 隐式捕获了 `this`。如果 `CNamedPipeClient` 对象在 `Disconnect()` 之前被析构（或 `Disconnect()` 在工作线程内被调用后走 `detach` 路径），线程仍在访问已释放的成员。`CNamedPipeClient.cpp:198` 的 `m_thread.detach()` 更加剧了这个问题 — detach 后线程的生命周期完全不受控。

---

## 中等问题 (Medium)

### 5. 裸指针管理 — `IPCServer::pServer` 和 `IPCClient::pClient`

**文件:** `IPCServer.h:31`, `IPCClient.h:24`

```cpp
CNamedPipeServer* pServer = nullptr;  // 应该用 unique_ptr
CNamedPipeClient* pClient = nullptr;  // 应该用 unique_ptr
```

手动 `new`/`delete` 容易在异常路径上泄漏。`IPCServer::Listen()` 中 `new(std::nothrow)` 返回 `nullptr` 时没问题，但 `IPCClient::Connect()` 中第 16 行用的是普通 `new`，如果 `CNamedPipeClient` 构造函数抛异常会泄漏。建议统一用 `std::unique_ptr`。

---

### 6. `DBG_ERROR` / `DBG_WARN` / `DBG_INFO` 在 Release 下全部静默

**文件:** `pch.h:16-24`

```cpp
#ifdef _DEBUG
#define DBG_LOG(fmt, ...) DbgPrintf(...)
#else
#define DBG_LOG(fmt, ...)    // Release 下全部为空
#endif
#define DBG_ERROR(fmt, ...) DBG_LOG(fmt, __VA_ARGS__)
```

Release 构建下 **所有** 日志（包括 `DBG_ERROR`）都被编译掉了。对于一个 IPC 框架，生产环境的错误日志是必要的排障手段。建议至少在 Release 下保留 `DBG_ERROR` 和 `DBG_WARN`，或者引入一个正式的日志库。

---

### 7. `IPCServerBroker::StopBroker()` — 时序竞争

**文件:** `IPCServerBroker.cpp:438-453`

```cpp
void IPCServerBroker::StopBroker()
{
    if (server)       // <-- 竞态：另一个线程可能同时进入
    {
        server->Stop();
        if (m_brokerThread.joinable())
        {
            m_brokerThread.join();
        }
        delete server;
        server = nullptr;
    }
}
```

`StopBroker()` 没有加锁保护，如果从多个线程同时调用（或析构函数和外部调用同时触发），`server` 指针可能出现 double-free。

---

### 8. `ipc_broker_start` 返回时 broker 可能尚未就绪

**文件:** `IPC.cpp:131`

```cpp
pServerBroker->RunBrokerAsync(pipe_name, onAuth);  // 异步启动
return pServerBroker;  // 立即返回
```

调用方拿到 handle 后立刻 `ipc_client_start` 连接，可能因为 broker 线程还没完成 `Listen()` 而连接失败。虽然客户端有重试逻辑（`WaitNamedPipe`），但这是一个隐式的时序依赖。

---

### 9. `IpcMessage` 构造函数中 `memset` 整个 8KB buffer

**文件:** `IPCMessage.h:50`

```cpp
memset(Data, 0, sizeof(Data)); // 每次构造都清零 ~8KB
```

对于高频消息场景，这 8KB 的 memset 是不必要的性能开销。消息在发送前会被完整覆盖，零初始化无实际意义。

---

### 10. `CNamedPipeServer::WritePipe()` 中 Overlapped 结果未等待

**文件:** `CNamedPipeServer.cpp:295-300`

```cpp
OVERLAPPED ovWrite = { 0 };
success = WriteFile(..., &ovWrite);
```

使用了 Overlapped I/O，但如果 `WriteFile` 返回 `TRUE`（同步完成），`bytesWritten` 可能是 0（对于 overlapped 操作这是合法的），代码却认为成功。此外，`ovWrite.hEvent` 未设置（为 `NULL`），依赖文件句柄的默认行为，这在某些边缘情况下可能导致 `GetOverlappedResult` 行为不确定。

---

## 低级问题 (Low)

### 11. `GetTickCount()` 作为消息 ID — 低分辨率且会回绕

**文件:** `IPCMessage.h:52`

```cpp
header.MsgId = GetTickCount(); // ~15ms 精度，49.7 天回绕
```

同一毫秒内的多条消息会拥有相同的 MsgId。建议用 `QueryPerformanceCounter` 或原子计数器。

---

### 12. `WaitNamedPipe` 硬编码 20 秒超时

**文件:** `CNamedPipeClient.cpp:77`

```cpp
if (!WaitNamedPipe(lpszPipeName, 20000))
```

20 秒是硬编码的，对于不同部署场景（本地 vs 远程、调试 vs 生产）可能不合适。建议作为参数传入或使用配置。

---

### 13. `IpcMessage` 构造函数抛异常 — 不应在 IPC 框架中使用异常

**文件:** `IPCMessage.h:63`

```cpp
throw std::runtime_error("Data size exceeds buffer size");
```

C++ 异常在 IPC 回调链路中传播，如果调用方没有 catch，直接 crash。返回错误码更符合系统编程惯例。

---

### 14. `IPCServer::Listen()` 的 `new(std::nothrow)` 与普通 `new` 混用

`IPCServer.cpp:20` 用 `new(std::nothrow)`，但 `IPCClient.cpp:16` 用普通 `new`。风格不一致。

---

### 15. `CNamedPipeServer` 使用 `LPTSTR` 参数（非 const）

**文件:** `CNamedPipeServer.h:13`

```cpp
CNamedPipeServer(LPTSTR lpszPipeName, ...);
```

应使用 `LPCTSTR`（const 指针），因为构造函数不会修改传入的管道名。

---

### 16. 安全描述符可收紧

**文件:** `CNamedPipeServer.cpp:82`

```cpp
TEXT("D:(A;;GA;;;AU)")  // Generic All for Authenticated Users
```

`GA` (Generic All) 权限过大，实际只需 `GR` (Generic Read) + `GW` (Generic Write)。对于 Broker 场景，可以进一步限制为特定 SID。

---

### 17. `spec.md` 要求的 `ipc_broker_start_async` 在 `IPC.h` 中未导出

spec.md 提到了 `Ipc_Broker_Start` 同步和异步两个版本，但 `IPC.h` 只导出了 `ipc_broker_start`（内部调用的是 `RunBrokerAsync`，但名字暗示是同步的）。命名有歧义。

---

## 总结

| 级别 | 数量 | 关键项 |
|---|---|---|
| **High** | 4 | ReadPipe 字节丢失、WaitForMultipleObjects 返回值未校验、线程 UAF 风险、int/unsigned long 类型混用 |
| **Medium** | 6 | 裸指针、Release 无日志、StopBroker 竞态、broker 启动时序、memset 性能、Overlapped 用法 |
| **Low** | 7 | GetTickCount、硬编码超时、异常使用、命名规范等 |

最值得优先修复的是 **问题 1（ReadPipe 字节丢失）** 和 **问题 3（WaitForMultipleObjects 返回值未校验）**，前者会导致消息数据损坏，后者会导致服务端崩溃。
