要求：
1. 使用命名管道
2. 有认证：文件签名认证，在broker的回调函数中认证

Client
```
ipc_msg_header
{
    ipc_sig,  //'IPCM'
    msg_id,   //timestamp
    src_id,
    dst_id,   //dst_id == 0, control message， dst_id == 0xffff, broadcast message
    msg_type,
    msg_len
}

ipc_msg
{
    ipc_msg_header,
    payload           //MAX_PAYLOAD_SIZE = 8192 - sizeof(ipc_msg_header)
}

msg_type
{
    //0~9, reserved for internal use
    0,  //client register
    1,  //dst not found
    2,  //kick: client replaced by another with same id
    3,  //invalid message, header verify failed
    4,  //msg size too large
    5~9,  //reserved
    10~0xFFFF,  //user define         
}

Callback(ipc_msg)
{
    if(msg_type < 10)  //internal message
    {
        return;
    }
    if(dst_id == 0xffff)  //broadcast message
    {
    }
    switch(msg_type)
    {
    }
}

Ipc_Connect(client_id, broker_name, Callback(ipc_msg));
Ipc_Register_Msg(ipc_broadcast_msg_type);   //可重复调用
Ipc_BroadCast(ipc_broadcast_msg);
Ipc_Send(client_dst_id, ipc_msg);
Ipc_Disconnect();
```
 - client_id: 如果出现重复，覆盖旧连接（踢掉旧client），Broker输出WARN日志
Broker
```
bool OnClientConnect(handle)
{
    bool bRet = Auth(handle);
    return bRet;
}

Ipc_Broker_Start(broker_name, OnClientConnect(client_pipe_handle), OnClientDisconnect(client_pipe_handle));
Ipc_Broker_Stop();
```
 -  内部进行消息中转
 -  单播中转，回复发送方消息是否送达
 -  广播中转，只向注册了消息的客户端发送消息