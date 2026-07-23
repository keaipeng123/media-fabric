# media-fabric

`media-fabric` 是一个 GB/T 28181 学习与流媒体工程化实践项目。当前仓库包含两个历史服务：

- `SipSupService`：上级平台历史目录，负责接收下级注册、处理心跳、发起目录查询和实时预览 INVITE，并接收 RTP/PS 媒体流。
- `SipSubService`：下级平台/设备模拟历史目录，负责向上级注册、发送心跳、响应目录/录像查询、处理 INVITE，并从本地 H.264 测试流封装 PS/RTP 后发送。

阶段 0 的工程目标是把这两个历史服务逐步整理为一个统一项目，最终形成单一服务入口 `media-fabric`。标准形态是**同一个进程内的 GB28181 节点同时具备注册客户端、注册服务端、心跳、目录、INVITE、媒体收发等能力**。上级/下级是某条级联关系中的相对身份，不应成为工程拆分边界。

## 当前能力

历史服务中已有能力：

- GB/T 28181 REGISTER 注册与 Digest 鉴权。
- MESSAGE keepalive 心跳。
- Catalog 目录查询与响应。
- RecordInfo 录像查询基础流程。
- INVITE/BYE 实时预览与停止。
- RTP/RTCP 会话创建，支持 UDP 与部分 TCP 主被动连接逻辑。
- PS mux/demux。
- H.264 裸流读取、PS 封装、RTP 发送。
- RTP/PS 接收、组包、PS 解封装、H.264 裸流提取。

统一入口 `media-fabric` 当前已具备的阶段性能力：

- 创建单一 `GB28181Node` 并同时装配注册客户端、注册服务端、心跳、目录、录像查询、INVITE、媒体收发能力模块。
- 通过 PJSIP adapter 承载真实 SIP 网络收发，并覆盖入站 SIP request 和 SIP response 到统一路由的转换。
- 通过 JRTPLIB RTP adapter 承载真实 RTP 会话，完成媒体发送、接收和统计。
- 通过 `StreamFileFrameSource` 读取历史 `stream.file` 测试流，`MediaSendCapability` 启动时会校验配置媒体源，并按 `[media] stream_send_interval_ms` 周期循环驱动 `sendAnnexBFrame`。
- 能生成 REGISTER、REGISTER Digest 认证重试、MESSAGE keepalive、Catalog、RecordInfo、INVITE 和 INVITE 2xx 后 ACK 出站请求意图。
- 能处理 REGISTER、Keepalive、Catalog、RecordInfo、INVITE、ACK、BYE 入站请求，并生成基础 SIP 响应或业务响应消息；REGISTER 已具备随机 401 Digest challenge、MD5 response 校验、nonce 缓存和 replay 拒绝边界。
- MESSAGE 已具备 MANSCDP XML 解析边界，可提取 root、CmdType、SN、DeviceID、Result、StartTime、EndTime、SumNum、DeviceList/RecordList Item；Catalog Item 当前覆盖 DeviceID、Name、Manufacturer、Model、Owner、CivilCode、Parental、ParentID、SafetyWay、RegisterWay、Secrecy、Status 等常用字段，Record Item 当前覆盖 FilePath、StartTime、EndTime 等常用字段，并校验 Keepalive/Catalog/RecordInfo 的 DeviceID 与列表一致性。
- Catalog/RecordInfo 响应明细已落入节点级业务状态，可查询当前目录项、录像项计数和按 peer 保存的最近一次明细快照，并可导出/恢复文件快照；`BusinessQueryService` 已提供协议无关的 JSON 查询边界，并已接入统一入口的 CLI 管理查询。
- INVITE 已具备轻量 SDP 解析、Call-ID/CSeq/Contact 基础关联、媒体会话记录、SDP 200 响应生成边界，以及收到 INVITE 2xx response 后生成 ACK 出站意图；ACK 会继承 response 的 Call-ID、CSeq、From/To tag，并可优先使用 Contact target。ACK request 会确认媒体会话。发送侧可将 Annex-B 帧封装为 PS payload，并通过 `RtpSessionAdapter::sendPayloadPacket` 切分发送 payload type 96 的 RTP payload packet；接收侧可按 session 更新接收统计和 `stream-receiving` 状态，按 timestamp/marker 重组 RTP payload，解析完整 PS pack/PES，识别视频 PES 中的 Annex-B H.264/H.265 NAL，并通过 `MediaFrameSink` 输出完整 Annex-B 帧到文件。bad SDP 会返回 400，错误 dialog 的 BYE 会返回 481，合法 BYE 会释放本地 RTP 端口。
- 已通过节点级任务注册 REGISTER refresh 和 keepalive 周期保活。

当前媒体发送配置位于 `conf/media-fabric.conf` 的 `[media]`：

```ini
stream_file = SipSubService/conf/stream.file
rtp_payload_bytes = 1300
rtp_timestamp_increment = 3600
stream_send_interval_ms = 1000
stream_loop = true
```

## 文档入口

- [项目式学习计划](docs/流媒体开发项目式学习计划.md)
- [阶段 0 执行计划](docs/阶段0-整理现有项目基础执行计划.md)
- [架构现状](docs/architecture.md)
- [里程碑 3 公共模块盘点](docs/milestone3-common-inventory.md)
- [里程碑 4 节点与能力模块](docs/milestone4-node-capabilities.md)
- [里程碑 4 完成审计清单](docs/milestone4-completion-audit.md)
- [信令流程](docs/signaling-flow.md)
- [媒体流程](docs/media-flow.md)
- [第三方库补齐说明](docs/third-party-libs.md)
- [抓包与排查](docs/wireshark.md)
- [历史笔记索引](docs/notes/README.md)

## 目录结构

```text
.
├── SipSupService/      # 上级平台角色历史工程
├── SipSubService/      # 下级平台/设备模拟角色历史工程
├── 3rd/                # 第三方头文件与静态库
├── docs/               # 项目文档与长期维护笔记
└── README.md
```

## 构建

当前已经具备根目录统一 `CMakeLists.txt`，默认构建目标是最终入口 `media-fabric`。

默认构建：

```bash
cmake -S . -B build
cmake --build build
```

默认构建启用 PJSIP 与 JRTPLIB，用于真实 SIP/RTP 通信。若此前使用过旧版本或旧项目路径的 `build` 目录，请先删除该目录后再执行配置。

## 本地管理命令行

`media-fabric` 是常驻服务；`mfcli` 通过 `[management] socket_path` 指定的本机 Unix Socket 连接服务，不直接发送 SIP 消息：

```bash
./build/media-fabric -c conf/media-fabric.conf
./build/mfcli peers
./build/mfcli register 10000000002000000001
./build/mfcli catalog-query 12000000002000000001
./build/mfcli catalog-show 12000000002000000001
./build/mfcli invite 11000000001310000059
./build/mfcli streams
./build/mfcli bye 11000000001310000059
```

注册成功后，服务会按 `[timers] heartbeat_interval_seconds` 自动发送心跳；`peers` 显示上下级节点的实时注册与最后心跳时间。
`invite` 的参数是已同步目录中的设备 `DeviceID`，不是下级平台 ID；服务会自动按目录归属路由到对应已注册下级。`streams` 显示 RTP 收流统计，`bye` 停止指定设备的开流会话。

本地模拟上下级可分别使用 `conf/media-fabric-sup.conf` 和 `conf/media-fabric-sub.conf`。先启动上级与下级，再从下级发起注册：

```bash
./build/media-fabric -c conf/media-fabric-sup.conf
./build/media-fabric -c conf/media-fabric-sub.conf
./build/mfcli --socket /tmp/media-fabric-sub.sock register 10000000002000000001
./build/mfcli --socket /tmp/media-fabric-sup.sock peers
```

使用 `mfcli` 测试时，先执行注册和目录查询，再执行开流与 `streams` 观察 RTP 统计；测试结束执行 `bye` 回收流会话。

历史服务目录仍可作为迁移对照目标配置，但不作为默认交付目标。第三方静态库以 Linux 目标为主，例如 `x86_64-unknown-linux-gnu`，因此历史目标建议在 Linux 环境验证。

历史目标配置示例：

```bash
cmake -S . -B build-legacy -DMEDIA_FABRIC_BUILD_LEGACY_SERVICES=ON
```

注意：当前配置文件和日志路径存在 `/home/media-fabric/...` 这类硬编码路径，阶段 0 后续会整理为统一配置。

## 配置现状

历史配置源仍分散在两个旧目录中，当前已新增 `conf/media-fabric.conf` 作为统一入口样例。统一配置描述一个本地 SIP 节点和多个 peer 关系：`[node]` 持有唯一的本地 SIP/RTP 配置，`[peer.upstream.*]` 描述需要注册到的远端平台，`[peer.downstream.*]` 描述允许接入或预期接入的下级设备。peer 中的 `remote_port` 是远端端口，不是本地监听端口。

当前统一入口默认本地节点：

- SIP ID：`11000000002000000001`
- SIP 地址：`127.0.0.1:7101`
- RTP 端口范围：`30000-40000`

当前统一入口默认 peer：

- 上级平台注册目标：`10000000002000000001@127.0.0.1:5061`
- 允许接入的下级设备：`12000000002000000001`

旧 `[sup]` / `[sub]` 配置段仅保留临时兼容，用于迁移期测试；默认交付模型不再把上级和下级实现为两个本地 SIP endpoint。

接收下级注册关系的历史配置：

- `SipSupService/conf/SipSupService.conf`
- 默认 SIP ID：`10000000002000000001`
- 默认 SIP 端口：`5061`
- 默认 RTP 端口范围：`20000-30000`

向上级注册关系的历史配置：

- `SipSubService/conf/SipSubService.conf`
- 默认 SIP ID：`11000000002000000001`
- 默认 SIP 端口：`7101`
- 默认 RTP 端口范围：`30000-40000`

测试媒体与目录数据：

- `SipSubService/conf/stream.file`
- `SipSubService/conf/catalog.json`

统一入口新增 `[media]` 段：

```ini
[media]
stream_file = SipSubService/conf/stream.file
rtp_payload_bytes = 1300
rtp_timestamp_increment = 3600
```

## 计划中的统一入口

阶段 0 后续目标：

```bash
./media-fabric -c conf/media-fabric.conf
```

统一进程启动后初始化 `GB28181Node`，再装配注册客户端、注册服务端、心跳、目录、INVITE、媒体接收、媒体发送等能力模块。`SipSupService` 和 `SipSubService` 仅作为迁移前的代码来源，后续完成整合后以 `media-fabric` 为唯一工程入口。
