# 架构现状

更新日期：2026-06-29

## 项目定位

当前项目是一个 GB/T 28181 学习工程，已经具备上下级信令交互和基础媒体收发能力。阶段 0 的目标不是马上新增播放协议，而是先把当前代码整理成后续 `gb28181-server` 单服务架构的底座。

## 当前角色

```mermaid
flowchart LR
  Client["本地控制客户端"] -->|"TCP command :11300"| Sup["SipSupService\n上级平台角色"]
  Sub["SipSubService\n下级平台/设备模拟角色"] -->|"REGISTER / MESSAGE"| Sup
  Sup -->|"Catalog / RecordInfo / INVITE / BYE"| Sub
  Sub -->|"RTP/PS payload type 96"| Sup
  Sub -->|"stream.file / catalog.json"| LocalFiles["本地测试数据"]
```

## SipSupService

职责：

- 监听上级平台 SIP 端口，默认 `5061`。
- 接收下级 REGISTER，并完成普通注册或 Digest 鉴权注册。
- 接收下级 MESSAGE keepalive。
- 通过本地 TCP 控制端口触发目录查询、开流等任务。
- 发起 Catalog 查询、INVITE、BYE。
- 创建 RTP 接收会话，接收 PS 负载并解封装出 H.264/H.265 裸流。

关键入口：

- 主入口：[SipSupService/src/main.cpp](../SipSupService/src/main.cpp)
- SIP 初始化与消息分发：[SipSupService/src/SipCore.cpp](../SipSupService/src/SipCore.cpp)
- 注册处理：[SipSupService/src/SipRegister.cpp](../SipSupService/src/SipRegister.cpp)
- 心跳处理：[SipSupService/src/SipHeartBeat.cpp](../SipSupService/src/SipHeartBeat.cpp)
- 目录查询：[SipSupService/src/GetCatalog.cpp](../SipSupService/src/GetCatalog.cpp)
- 开流 INVITE：[SipSupService/src/OpenStream.cpp](../SipSupService/src/OpenStream.cpp)
- RTP/PS 接收与解封装：[SipSupService/src/Gb28181Session.cpp](../SipSupService/src/Gb28181Session.cpp)
- 本地 TCP 命令入口：[SipSupService/src/EventMsgHandle.cpp](../SipSupService/src/EventMsgHandle.cpp)

## SipSubService

职责：

- 监听下级平台 SIP 端口，默认 `7101`。
- 定时向上级 REGISTER。
- 注册成功后定时发送 MESSAGE keepalive。
- 响应 Catalog、RecordInfo 查询。
- 响应上级 INVITE，生成 SDP 200 OK。
- 从 `stream.file` 读取测试帧，封装为 PS，再通过 RTP 发送给上级。
- 处理上级 BYE，停止推流。

关键入口：

- 主入口：[SipSubService/src/main.cpp](../SipSubService/src/main.cpp)
- SIP 初始化与消息分发：[SipSubService/src/SipCore.cpp](../SipSubService/src/SipCore.cpp)
- 注册发起：[SipSubService/src/SipRegister.cpp](../SipSubService/src/SipRegister.cpp)
- 心跳发送：[SipSubService/src/SipHeartBeat.cpp](../SipSubService/src/SipHeartBeat.cpp)
- 目录响应：[SipSubService/src/SipDirectory.cpp](../SipSubService/src/SipDirectory.cpp)
- INVITE/BYE 处理与本地帧读取：[SipSubService/src/SipGbPlay.cpp](../SipSubService/src/SipGbPlay.cpp)
- PS 封装与 RTP 发送：[SipSubService/src/Gb28181Session.cpp](../SipSubService/src/Gb28181Session.cpp)

## 当前数据流

```text
SipSubService/conf/stream.file
  -> SipSubService SipGbPlay::recvFrame
  -> SipPsCode::incomeVideoData
  -> libmpeg ps_muxer_input
  -> SipPsCode::sendPackData
  -> jrtplib RTP SendPacket payload type 96
  -> SipSupService Gb28181Session::ProcessRTPPacket
  -> OnRTPPacketProcPs
  -> libmpeg ps_demuxer_input
  -> ps_demux_callback
  -> H.264/H.265 裸流处理
```

## 当前配置

上级配置：[SipSupService/conf/SipSupService.conf](../SipSupService/conf/SipSupService.conf)

- SIP ID：`10000000002000000001`
- SIP IP：`127.0.1`
- SIP Port：`5061`
- RTP 端口范围：`20000-30000`
- 下级节点：`11000000002000000001@127.0.1:7101`

下级配置：[SipSubService/conf/SipSubService.conf](../SipSubService/conf/SipSubService.conf)

- SIP ID：`11000000002000000001`
- SIP IP：`127.0.1`
- SIP Port：`7101`
- RTP 端口范围：`30000-40000`
- 上级节点：`10000000002000000001@127.0.1:5061`

## 工程化问题清单

- 根目录还没有统一构建入口。
- `SipSupService` 与 `SipSubService` 存在大量重复基础类，如 `ConfReader`、`ECSocket`、`ECThread`、`ThreadPool`、`TaskTimer`、`XmlParser`。
- 配置文件、日志目录、目录 JSON 读取存在硬编码绝对路径。
- 上下级角色都使用全局控制对象，后续单进程 `both` 模式需要隔离角色上下文。
- `Gb28181Session` 同时承载收流和发流语义，后续建议拆为接收会话和发送会话。
- 当前 `OpenStream` 中设备 ID、平台 ID、流类型存在硬编码，后续需要由命令或配置驱动。

## 目标架构

```text
gb28181-server
  common/
    config, socket, thread, timer, xml, sip message
  sip/
    sip core, register, heartbeat, catalog, invite
  media/
    rtp receive, rtp send, ps mux, ps demux, h264/h265 parser
  roles/
    SupServiceRole
    SubServiceRole
```

阶段 0 后续里程碑会先保留两个历史目标，再逐步抽公共库，最后新增单一二进制入口。
