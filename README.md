# Windows-cpp-rudp

> 在Windows下使用CPP编写的可靠UDP协议实现

## 实现以下功能：

- 建立连接三次握手
- 差错检测：检查消息类型、序列号、校验和
- 确认重传：包括差错重传和超时重传
- 流量控制：停等机制
- 断开连接四次握手

对文件传输进行了测试

## 编译

- 克隆项目到本地：

```bash
    git clone https://github.com/pqcqaq/windows-cpp-rudp.git
```

- 进入项目目录：

```bash 
    cd Windows-cpp-rudp
```

- 进行编译

```bash
    powershell ./build.ps1
```

## 使用编译后的可执行文件：

简单文本传输：
- 使用./server-hello \<port\> 的形式启动服务器.
- 使用./client-hello \<host\>:\<port\>的形式来打开客户端

在windows下相关系统api调用不如linux简洁，下面就不再编写其他的例子了
