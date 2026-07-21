# 里程碑 4：节点与能力模块封装

更新日期：2026-07-01

## 当前进展

已新增统一节点骨架：

```text
src/core/
  BusinessQueryService.h
  BusinessQueryService.cpp
  BusinessState.h
  BusinessState.cpp
  Capability.h
  GB28181Node.h
  GB28181Node.cpp
  ManscdpMessage.h
  ManscdpMessage.cpp
  NodeConfig.h
  NodeConfig.cpp
  NodeRuntime.h
  SipEndpoint.h
  SipMessageContext.h
  SipStack.h
  SipStack.cpp
  SipTransport.h
  SipTransport.cpp
  PeerRegistry.h
  PeerRegistry.cpp
  SessionManager.h
  SessionManager.cpp
  TaskScheduler.h
  TaskScheduler.cpp
src/media/
  ElementaryStream.h
  ElementaryStream.cpp
  MediaFrameSink.h
  MediaFrameSink.cpp
  MediaManager.h
  MediaManager.cpp
  ProgramStream.h
  ProgramStream.cpp
  RtpPacket.h
  RtpPacket.cpp
  RtpSessionAdapter.h
  RtpSessionAdapter.cpp
  SdpSession.h
  SdpSession.cpp
  StreamFileFrameSource.h
  StreamFileFrameSource.cpp
```

已新增标准能力模块装配入口：

```text
src/capabilities/
  LifecycleCapability.h
  LifecycleCapability.cpp
  GB28181Capabilities.h
  GB28181Capabilities.cpp
  SipTask.h
  StandardCapabilities.h
  StandardCapabilities.cpp
```

已新增可替换的 SIP 传输适配边界：

```text
src/core/
  SipTransport.h
  SipTransport.cpp
src/adapters/pjsip/
  PjsipStackAdapter.h
  PjsipStackAdapter.cpp
```

默认构建使用 `InMemorySipTransport` 支撑内部自测；Linux 目标环境可通过 `-DMEDIA_FABRIC_ENABLE_PJSIP=ON` 打开实验性的 PJSIP UDP/TCP 监听 adapter。这个 adapter 已经能把 PJSIP 收到的 SIP request 和 SIP response 转换为统一的 `SipRequestContext`，再交给能力模块路由处理；能力模块发出的 `SipMessageContext` 可由传输层转换为出站 SIP 请求或请求上下文内的 stateless response。

已新增可替换的 RTP 会话适配边界：

```text
src/media/
  RtpSessionAdapter.h
  RtpSessionAdapter.cpp
src/adapters/jrtplib/
  JrtplibRtpSessionAdapter.h
  JrtplibRtpSessionAdapter.cpp
```

默认自测通过 `RtpSessionAdapter` 抽象入口把 RTP 包送入 `MediaManager`，验证适配器收包入口与直接注入 RTP 入口共用同一条 RTP/PS/ES/帧聚合链路；Linux 目标环境可通过 `-DMEDIA_FABRIC_ENABLE_JRTPLIB=ON` 打开实验性的 JRTPLIB RTP adapter。当前仓库 `3rd/lib` 有 `libjthread.a`，但缺少 `libjrtp.a`，所以 JRTPLIB 真实链接、启动和抓包验证仍需在 Linux 环境补齐依赖后执行。

当前 `media-fabric` 启动流程已经从 bootstrap 文案变为：

```text
main()
  -> load NodeConfig
  -> create GB28181Node
  -> create standard capabilities
  -> configure SipStack / PeerRegistry / MediaManager
  -> start GB28181Node
  -> register SIP routes from capabilities
  -> stop GB28181Node
```

## 已装配能力

```text
RegisterClientCapability
RegisterServerCapability
KeepaliveClientCapability
KeepaliveServerCapability
CatalogClientCapability
CatalogServerCapability
RecordQueryClientCapability
RecordQueryServerCapability
InviteClientCapability
InviteServerCapability
MediaReceiveCapability
MediaSendCapability
```

当前能力模块已具备生命周期、运行上下文注入、SIP 路由注册、出站 SIP 消息生成能力。RTP 包解析、RTP payload 分片发送边界、RTP adapter 发送入口、RTP payload 按 marker 重组、PS pack/PES 基础解析、Annex-B H.264/H.265 NAL 识别、历史 `stream.file` 循环读取、Annex-B 帧文件输出、媒体接收状态统计和 RTP 会话适配边界已迁入；真实网络抓包验证和 JRTPLIB 真实收发尚未完成。

已注册 SIP 路由：

```text
REGISTER/request              -> RegisterServerCapability
REGISTER/response             -> RegisterClientCapability
MESSAGE/Notify/keepalive      -> KeepaliveServerCapability
MESSAGE/Response/Catalog      -> CatalogClientCapability
MESSAGE/Query/Catalog         -> CatalogServerCapability
MESSAGE/Response/RecordInfo   -> RecordQueryClientCapability
MESSAGE/Query/RecordInfo      -> RecordQueryServerCapability
INVITE/response               -> InviteClientCapability
INVITE/request                -> InviteServerCapability
ACK/request                   -> InviteServerCapability
BYE/request                   -> InviteServerCapability
```

已迁入的新节点内部业务状态：

```text
RegisterClientCapability
  -> 为 upstream peer 创建 pending-register 会话
  -> 向 upstream peer 生成 REGISTER 出站请求
  -> 收到 REGISTER/response 401 时按 WWW-Authenticate 生成带 Authorization 的 REGISTER 重试
  -> 注册 300 秒周期 REGISTER refresh 任务

RegisterServerCapability
  -> 未认证 REGISTER 且目标 endpoint 配置 realm/password 时生成随机 401 Digest challenge
  -> 缓存 Digest challenge，维护 300 秒过期边界
  -> Digest Authorization nonce/username/realm/response 校验失败时生成 403
  -> 已使用的无 qop nonce 会拒绝重放
  -> 带 qop/nc 的 Digest 会校验 nc 单调递增，重复 nc 会拒绝重放
  -> Digest Authorization MD5 response 校验通过后才更新注册状态
  -> REGISTER expires > 0 时标记 peer registered
  -> REGISTER expires = 0 时标记 peer unregistered
  -> 生成 REGISTER 200/403 响应

KeepaliveClientCapability
  -> 向 upstream peer 生成 MESSAGE/Notify/keepalive 出站请求
  -> 注册 60 秒周期 keepalive 任务

KeepaliveServerCapability
  -> MESSAGE/Notify/keepalive 时解析 MANSCDP XML 并校验 DeviceID 为发送方
  -> XML root/CmdType/DeviceID 不合法时生成 400 响应
  -> XML 校验通过时更新 peer lastKeepaliveTime
  -> 生成 keepalive 200/403 响应

CatalogClientCapability
  -> 向 downstream peer 生成 MESSAGE/Query/Catalog 出站请求
  -> MESSAGE/Response/Catalog 时解析 MANSCDP XML 并校验 DeviceID 为发送方
  -> SumNum > 0 时要求 DeviceList 至少解析出一个 Item
  -> XML 校验通过时创建 catalog-response-received 会话并记录 sum/items/first
  -> XML 校验通过时将 DeviceList Item 落入 BusinessState

CatalogServerCapability
  -> MESSAGE/Query/Catalog 时解析 MANSCDP XML 并校验 DeviceID 指向已知 endpoint
  -> XML root/CmdType/DeviceID 不合法时生成 400 响应
  -> XML 校验通过时创建 catalog-query-received 会话
  -> 生成 Catalog 200 响应和 MESSAGE/Response/Catalog 出站消息

RecordQueryClientCapability
  -> 向 downstream peer 生成 MESSAGE/Query/RecordInfo 出站请求
  -> MESSAGE/Response/RecordInfo 时解析 MANSCDP XML 并校验 DeviceID 为发送方
  -> SumNum > 0 时要求 RecordList 至少解析出一个 Item
  -> XML 校验通过时创建 record-response-received 会话并记录 sum/items/first
  -> XML 校验通过时将 RecordList Item 落入 BusinessState

RecordQueryServerCapability
  -> MESSAGE/Query/RecordInfo 时解析 MANSCDP XML 并校验 DeviceID 指向已知 endpoint
  -> XML root/CmdType/DeviceID 不合法时生成 400 响应
  -> XML 校验通过时创建 record-query-received 会话
  -> 生成 RecordInfo 200 响应和 MESSAGE/Response/RecordInfo 出站消息

InviteClientCapability
  -> 向 downstream peer 生成带 SDP body 的 INVITE 出站请求
  -> 收到 INVITE/response 2xx 时生成 ACK 出站请求，并继承 response 的 Call-ID/CSeq、From/To tag 和 Contact target

InviteServerCapability
  -> INVITE/request 时解析 SDP，提取 remote RTP 地址、端口、传输协议、方向和 SSRC
  -> SDP 解析失败时生成 400 响应并释放 RTP 端口
  -> SDP 解析成功时分配 RTP 端口并按 Call-ID 创建 stream-requested 会话和媒体会话
  -> 媒体会话记录 Call-ID、INVITE CSeq 和远端 Contact
  -> 生成带 SDP body 的 INVITE 200 响应
  -> ACK/request 时确认已存在的媒体会话，更新为 stream-confirmed
  -> 无对应 INVITE 媒体会话或 CSeq 不匹配的 ACK/request 会被拒绝
  -> 发送侧可按 media session 将 Annex-B 帧封装为 PS payload，再按最大 RTP payload 字节数切分，并通过 RTP adapter 发送 payload type 96 的 RTP payload packet
  -> 接收侧 RTP packet 可按 media session 校验 SSRC，按 timestamp/marker 聚合 RTP payload，marker 到达后解析完整 PS pack/PES 基础结构，识别视频 PES 内的 Annex-B H.264/H.265 NAL，输出完整 Annex-B 帧到 MediaFrameSink，更新 seq/timestamp/packet/bytes/PS/PES/NAL/frame 统计，并更新为 stream-receiving
  -> BYE/request 时关闭开流会话和媒体会话，释放本地 RTP 端口，并记录 stream-stopped
  -> 无对应 dialog 的 BYE/request 生成 481 响应
  -> 生成 BYE 200 响应

MediaSendCapability
  -> 启动时读取 `[media] stream_file`，校验历史测试流源可用并记录首个视频帧 pts/bytes/keyFrame
  -> 注册 media-send 周期任务，扫描 stream-confirmed/stream-receiving 且 RTP adapter running 的媒体会话
  -> 可通过 MediaManager 将 Annex-B 帧封 PS 并经 RTP adapter 发送分片 payload packet
```

## 设计边界

- `GB28181Node` 是唯一运行节点，不拆 `SupContext` / `SubContext`。
- 上级/下级只作为对端关系存在，不作为工程主模块边界。
- 能力模块通过 `NodeRuntime` 访问节点配置、SIP 栈、对端注册表、会话管理器、任务调度器和媒体管理器。
- `TaskTimer` 被新的 `TaskScheduler` 生命周期边界替代。
- `SipTaskBase` 被新的 `SipTask` 请求任务边界替代。
- `SipStack` 不直接绑定 PJSIP，而是依赖 `SipTransport`。默认内存传输用于非 Linux 编辑环境和自测；PJSIP adapter 用于后续 Linux 真实网络验证。
- 能力模块只生成 `SipMessageContext`，不直接包含 PJSIP 类型；PJSIP request/response 细节集中在 adapter。
- `BusinessState` 维护 Catalog/RecordInfo 响应明细的节点内快照，当前按 peerId 保存最近一次 DeviceList/RecordList Item，并由 `GB28181Node` 暴露计数、快照查询、导出/恢复和保存/加载文件快照入口。
- `BusinessQueryService` 提供协议无关的业务状态 JSON 查询边界，当前可输出 summary、Catalog peer 明细和 RecordInfo peer 明细；统一入口已通过 `--business-query summary|catalog|record --business-state-file path [--peer-id sip_id]` 接入 CLI 管理查询。
- `SipMessageContext` 已携带本地/远端地址、Expires、Call-ID、CSeq、Contact、From/To tag、Digest challenge 参数和出站 Digest Authorization 字段，PJSIP adapter 可据此生成 REGISTER Expires、From/Contact、WWW-Authenticate、Authorization，并在 ACK 等出站请求中带上指定 Call-ID/CSeq/From tag/To tag，且优先使用 Contact 中的 SIP URI 作为请求目标。
- `SipRequestContext` 已携带 Digest Authorization/challenge 字段；PJSIP adapter 会把 Authorization header 解析为 username、realm、nonce、uri、response、algorithm、opaque、qop、nc、cnonce，并把 WWW-Authenticate 解析为 Digest challenge。
- `SipRequestContext` 已携带 Call-ID、CSeq、Contact、From/To tag、SIP status code 和 reason 字段；PJSIP adapter 会从真实 SIP header/status line 中提取这些字段。入站 SIP response 会按 CSeq method 生成 `method=<CSeq.method>, event=response` 的统一上下文。
- `SipRequestContext` 已携带 `ManscdpMessage` 字段；PJSIP adapter 收到 MESSAGE 时会解析 XML body 并用 root/CmdType 生成 `event`。
- `ManscdpMessage` 是轻量 MANSCDP XML 边界，当前提取 root、CmdType、SN、DeviceID、Result、StartTime、EndTime、SumNum，并解析 DeviceList/RecordList 下的 Item 明细。Catalog Item 当前覆盖 DeviceID、Name、Manufacturer、Model、Owner、CivilCode、Parental、ParentID、SafetyWay、RegisterWay、Secrecy、Status、Longitude、Latitude；Record Item 当前覆盖 DeviceID、Name、FilePath、StartTime、EndTime 以及复用的设备描述字段。
- `RegisterServerCapability` 自己维护 Digest challenge 缓存，不把认证临时态混入 peer registry；无 qop nonce 一次性使用，带 qop/nc 的请求要求 nc 单调递增。
- `SdpSession` 是 PJSIP-free 的轻量 SDP 边界，当前覆盖 `o=`、`s=`、`c=`、`t=`、`m=video`、方向属性、`a=setup` 和 `y=`。
- `RtpPacket` 是 PJSIP-free/JRTPLIB-free 的 RTP v2 包边界，当前支持固定头、CSRC、extension、padding、payload 解析、测试包构建和 RTP payload 分片打包。
- `ProgramStream` 是媒体层的 PS payload 边界，当前支持 pack header、system header、program stream map、PES 包、视频/音频/private PES 和 PES payload byte 统计。
- `ElementaryStream` 是媒体层的 Annex-B 视频裸流边界，当前支持 H.264/H.265 NAL 识别，并统计 H.264 SPS/PPS/IDR 与 H.265 VPS/SPS/PPS/IDR。
- `MediaFrameSink` 是媒体层的帧输出边界，当前提供文件 sink，可把聚合后的 Annex-B 帧写入文件。
- `StreamFileFrameSource` 是媒体层的历史测试流输入边界，当前读取旧 `stream.file` 中的 `StreamHeader + frame payload`，跳过非视频帧并返回视频帧数据；EOF 后可由发送能力按配置重开文件实现循环发送。
- `RtpSessionAdapter` 是媒体层真实 RTP 会话适配边界，`MediaManager` 只依赖该抽象，不直接依赖 JRTPLIB 类型；当前边界覆盖 RTP 收包回调和 RTP payload packet 发送。
- `JrtplibRtpSessionAdapter` 是实验性 JRTPLIB 实现，负责在真实 UDP RTP 会话收到包后回调统一 RTP 字节流，并可通过 `SendPacket` 发送 RTP payload packet；该实现默认不参与构建，只在 `MEDIA_FABRIC_ENABLE_JRTPLIB=ON` 时编译链接。
- `MediaManager` 已维护 RTP 端口池和轻量媒体会话表；INVITE 成功后记录本地/远端 RTP、传输协议、方向、SSRC、Call-ID、INVITE CSeq 和远端 Contact，ACK 后确认会话。发送侧可把 Annex-B 帧封 PS 并通过 RTP adapter 分片发送；`MediaSendCapability` 会按 `[media] stream_send_interval_ms` 扫描已确认/接收中的媒体会话，从 `StreamFileFrameSource` 取下一帧并循环驱动 `sendAnnexBFrame`。接收侧可对同 timestamp 的 RTP payload 按 marker 重组完整 PS，再解析 PS/PES/H.264/H.265，输出完整 Annex-B 帧并进入 `stream-receiving` 状态，BYE 后停止 RTP adapter 并释放端口。
- `TaskScheduler` 支持毫秒/秒级周期任务和可唤醒停止，节点停止时先停周期任务，再停止能力模块，避免长间隔任务拖慢退出。
- 当前阶段不修改 `SipSupService/src`、`SipSupService/include`、`SipSubService/src`、`SipSubService/include`。

## 验证

已执行：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -R media-fabric-self-test
./build/media-fabric -c conf/media-fabric.conf
./build/media-fabric -c conf/media-fabric.conf --self-test
scripts/verify-milestone4-linux.sh preflight
scripts/verify-milestone4-linux.sh default
```

运行输出确认：

- `media-fabric` 成功创建 `GB28181Node`。
- 统一配置中的 `[node]` 被解析为唯一 SIP endpoint。
- `PeerRegistry` 从 `[peer.upstream.*]` / `[peer.downstream.*]` 能识别 1 个 upstream 和 1 个 downstream。
- `MediaManager` 能建立 RTP 端口池。
- 标准能力模块按顺序启动，并注册 11 条 SIP 路由。
- `--self-test` 能将 REGISTER response 401 自动认证重试、REGISTER challenge、bad REGISTER Digest、valid REGISTER Digest、REGISTER replay、qop REGISTER replay、bad Keepalive XML、valid Keepalive XML、Catalog、RecordInfo、bad Catalog Response、Catalog Response、bad RecordInfo Response、RecordInfo Response、INVITE SIP response、INVITE 2xx ACK、bad INVITE SDP、early ACK、valid INVITE SDP、bad ACK CSeq、ACK、wrong dialog BYE、BYE 请求分发到对应能力模块，并创建会话。
- `--self-test` 已覆盖 REGISTER response 401 -> Authorization REGISTER retry、REGISTER 401 challenge、REGISTER 403、REGISTER 200、REGISTER nonce replay reject、REGISTER qop/nc replay reject、Keepalive XML 400、Keepalive 200、Catalog、RecordInfo、Catalog/RecordInfo 列表响应解析与坏响应拒绝、Catalog 常用字段（含 Parental）落地、RecordInfo 明细落地、快照查询、导出恢复、文件持久化加载、JSON 查询和 CLI 查询分发、INVITE SIP response 分发、INVITE 2xx 后 ACK 出站、INVITE 400、early ACK reject、INVITE 200 SDP、ACK CSeq reject、ACK confirm、历史 `stream.file` 首帧读取、bad RTP SSRC reject、RTP packetize、RTP receive、RTP payload marker 重组、RTP adapter receive、RTP adapter send、PS parse、H.264 NAL parse、frame file output、BYE 481、BYE 200 的内部路由、状态变更、出站消息生成和 RTP 端口释放。
- `--self-test` 当前输出 `REGISTER_AUTH_RETRY=ok INVITE_RESPONSE=ok INVITE_ACK=ok routes=11 sent_messages=24 scheduled_tasks=3 catalog_items=1 record_items=1 catalog_snapshot=ok record_snapshot=ok business_state_restore=ok business_state_file=ok business_query_json=ok business_query_cli=ok MEDIA_SOURCE=ok MEDIA_SOURCE_PS=ok BAD_RTP_SSRC=rejected RTP_PACKETIZE=ok RTP=ok PS=ok H264=ok FRAME=ok FRAME_FILE=ok RTP_ADAPTER=ok SEND_ADAPTER=ok MEDIA_RECEIVING=ok MD5=ok sessions=24 media_sessions=0 endpoints=1 upstream_peers=1 downstream_peers=1 available_rtp_ports=5001`，包含启动阶段客户端请求、请求处理阶段服务端响应/业务响应、REGISTER refresh、keepalive 与 media-send 毫秒级周期任务注册。
- CTest 已注册 `media-fabric-self-test`，默认使用 `conf/media-fabric.conf`，可通过 `-DMEDIA_FABRIC_SELF_TEST_CONFIG=path/to/conf` 覆盖；测试用 `REGISTER_AUTH_RETRY=ok`、`INVITE_ACK=ok`、`routes=11`、`sent_messages=24`、`sessions=24`、`endpoints=1`、`upstream_peers=1`、`downstream_peers=1` 作为通过标记。`scripts/verify-milestone4-linux.sh preflight` 可检查目标环境命令、配置、媒体源和第三方静态库是否齐备，设置 `GB28181_PREFLIGHT_STRICT=1` 后可作为 Linux 硬门禁。
- 停止时能力模块按反向顺序释放。

PJSIP adapter 在当前非 Linux 编辑环境下已执行接口级语法检查：

```bash
c++ -std=c++11 -Wall -Wextra -Wpedantic \
  -DPJ_M_X86_64=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 \
  -Isrc/core -Isrc/common/include -Isrc/media -Isrc/capabilities \
  -Isrc/adapters/pjsip -I3rd/include -I3rd/include/pjsip \
  -DGB28181_ENABLE_PJSIP \
  -c src/adapters/pjsip/PjsipStackAdapter.cpp -o /private/tmp/PjsipStackAdapter.o
```

该检查通过，仅 PJSIP bundled header 中存在注释编码 warning。最终仍需要 Linux 链接和抓包验证。

新增构建开关：

```bash
scripts/verify-milestone4-linux.sh pjsip-build
scripts/verify-milestone4-linux.sh pjsip-smoke
```

该开关依赖当前仓库 `3rd/lib` 中的 Linux PJSIP 静态库和系统 OpenSSL/ALSA。当前编辑环境不是 Linux，因此真实 PJSIP adapter 还需要在目标 Linux 开发环境完成编译、smoke 启动和抓包验证。

JRTPLIB adapter 在当前编辑环境下已执行接口级语法检查：

```bash
c++ -std=c++11 -Wall -Wextra -Wpedantic \
  -Isrc/media -I3rd/include \
  -c src/adapters/jrtplib/JrtplibRtpSessionAdapter.cpp \
  -o /private/tmp/JrtplibRtpSessionAdapter.o
```

真实 JRTPLIB RTP 入口后续在 Linux 上按以下方式验证：

```bash
scripts/verify-milestone4-linux.sh jrtplib-build
scripts/verify-milestone4-linux.sh jrtplib-smoke
```

该开关依赖 `3rd/lib/libjrtp.a` 和 `3rd/lib/libjthread.a`。当前仓库已有 `libjthread.a`，但缺少 `libjrtp.a`，因此真实 JRTPLIB 链接、smoke 启动和 RTP 抓包验证需要在 Linux 环境补齐依赖后继续；补齐步骤见 [third-party-libs.md](third-party-libs.md)。

最终真实网络组合需要同时启用 PJSIP 与 JRTPLIB：

```bash
scripts/verify-milestone4-linux.sh full-build
scripts/verify-milestone4-linux.sh full-smoke
scripts/verify-milestone4-linux.sh full-capture
```

该组合会生成 `build-linux-full/media-fabric`，用于后续同一进程内的真实 SIP + RTP 抓包验收。`full-capture` 会把 SIP/RTP pcap 输出到 `artifacts/milestone4/`。

## 未完成项

完成状态以 [milestone4-completion-audit.md](milestone4-completion-audit.md) 的逐项门禁为准。

里程碑 4 目前完成的是节点与能力模块边界、SIP 请求/响应意图生成，以及部分不依赖 PJSIP 的内部业务状态迁移。真实网络和媒体收发仍未完成：

```text
PJSIP adapter 的 Linux 链接、启动和抓包验证
REGISTER Digest 自动重注册和 replay 拒绝的 Linux 真实抓包验证；代码侧 REGISTER 401 response -> Authorization REGISTER retry 已完成
Catalog/RecordInfo 更完整的扩展字段语义，以及按需要继续补 HTTP 管理入口；CLI 管理查询已完成，Catalog 常用字段已覆盖到 Parental
INVITE 2xx response 后 ACK 发送、From/To tag 传播和 Contact target 选择代码侧已完成；ACK/BYE 真实抓包验证和错误响应兼容性验证仍需在 Linux 目标环境继续
JRTPLIB adapter 的 Linux 链接、启动和真实 RTP 收发抓包验证
PJSIP + JRTPLIB 同进程 full-smoke 和 SIP/RTP 联合抓包验证
```

`StreamFileFrameSource` 循环驱动 `sendAnnexBFrame` 的代码侧边界已完成；真实 RTP 网络输出仍需要在 Linux 目标环境里随 JRTPLIB adapter 逐条抓包验证。
