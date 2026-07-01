# GB28181-Server

`GB28181-Server` 是一个 GB/T 28181 学习与流媒体工程化实践项目。当前仓库包含两个历史服务：

- `SipSupService`：上级平台历史目录，负责接收下级注册、处理心跳、发起目录查询和实时预览 INVITE，并接收 RTP/PS 媒体流。
- `SipSubService`：下级平台/设备模拟历史目录，负责向上级注册、发送心跳、响应目录/录像查询、处理 INVITE，并从本地 H.264 测试流封装 PS/RTP 后发送。

阶段 0 的工程目标是把这两个历史服务逐步整理为一个统一项目，最终形成单一服务入口 `gb28181-server`。标准形态是**同一个进程内的 GB28181 节点同时具备注册客户端、注册服务端、心跳、目录、INVITE、媒体收发等能力**。上级/下级是某条级联关系中的相对身份，不应成为工程拆分边界。

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

统一入口 `gb28181-server` 当前已具备的阶段性能力：

- 创建单一 `GB28181Node` 并同时装配注册客户端、注册服务端、心跳、目录、录像查询、INVITE、媒体收发能力模块。
- 通过 `SipTransport` 抽象承载 SIP 收发，默认内存传输用于自测，Linux 可通过 `GB28181_ENABLE_PJSIP` 打开实验性 PJSIP adapter。
- 通过 `RtpSessionAdapter` 抽象承载真实 RTP 会话，默认自测使用适配器入口验证 RTP/PS/ES/帧输出和 adapter 发送链路，Linux 可通过 `GB28181_ENABLE_JRTPLIB` 打开实验性 JRTPLIB RTP adapter。
- 通过 `StreamFileFrameSource` 读取历史 `stream.file` 测试流，`MediaSendCapability` 启动时会校验配置媒体源并记录首个视频帧状态。
- 能生成 REGISTER、MESSAGE keepalive、Catalog、RecordInfo、INVITE 出站请求意图。
- 能处理 REGISTER、Keepalive、Catalog、RecordInfo、INVITE、ACK、BYE 入站请求，并生成基础 SIP 响应或业务响应消息；REGISTER 已具备随机 401 Digest challenge、MD5 response 校验、nonce 缓存和 replay 拒绝边界。
- MESSAGE 已具备 MANSCDP XML 解析边界，可提取 root、CmdType、SN、DeviceID、Result、StartTime、EndTime、SumNum、DeviceList/RecordList Item，并校验 Keepalive/Catalog/RecordInfo 的 DeviceID 与列表一致性。
- Catalog/RecordInfo 响应明细已落入节点级业务状态，可查询当前目录项、录像项计数和按 peer 保存的最近一次明细快照，并可导出/恢复文件快照；`BusinessQueryService` 已提供协议无关的 JSON 查询边界，后续可直接接 HTTP/CLI 管理入口。
- INVITE 已具备轻量 SDP 解析、Call-ID/CSeq/Contact 基础关联、媒体会话记录和 SDP 200 响应生成边界；ACK 会确认媒体会话。发送侧可将 Annex-B 帧封装为 PS payload，并通过 `RtpSessionAdapter::sendPayloadPacket` 切分发送 payload type 96 的 RTP payload packet；接收侧可按 session 更新接收统计和 `stream-receiving` 状态，按 timestamp/marker 重组 RTP payload，解析完整 PS pack/PES，识别视频 PES 中的 Annex-B H.264/H.265 NAL，并通过 `MediaFrameSink` 输出完整 Annex-B 帧到文件。bad SDP 会返回 400，错误 dialog 的 BYE 会返回 481，合法 BYE 会释放本地 RTP 端口。
- 已通过节点级任务注册 REGISTER refresh 和 keepalive 周期保活。
- `--self-test` 可验证 10 条 SIP 路由、REGISTER Digest 接受/拒绝、REGISTER nonce replay 拒绝、MESSAGE XML 接受/拒绝、Catalog/RecordInfo 列表响应解析、业务状态快照查询、文件持久化恢复和 JSON 查询、INVITE SDP 接受/拒绝、ACK 确认、历史测试流首帧读取和 PS 回环解析、RTP packetize、RTP 包解析、RTP payload marker 重组、RTP adapter 收包入口、RTP adapter 发送入口、PS/PES 基础解析、H.264/H.265 NAL 识别、Annex-B 帧文件输出和媒体接收状态、peer 注册/心跳状态、RTP 端口分配/释放、出站 SIP 消息计数和周期任务注册。

## 文档入口

- [项目式学习计划](docs/流媒体开发项目式学习计划.md)
- [阶段 0 执行计划](docs/阶段0-整理现有项目基础执行计划.md)
- [架构现状](docs/architecture.md)
- [里程碑 3 公共模块盘点](docs/milestone3-common-inventory.md)
- [里程碑 4 节点与能力模块](docs/milestone4-node-capabilities.md)
- [信令流程](docs/signaling-flow.md)
- [媒体流程](docs/media-flow.md)
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

当前已经具备根目录统一 `CMakeLists.txt`，默认构建目标是最终入口 `gb28181-server`。

默认构建：

```bash
cmake -S . -B build
cmake --build build
```

启用 Linux PJSIP adapter：

```bash
cmake -S . -B build-linux-pjsip -DGB28181_ENABLE_PJSIP=ON
cmake --build build-linux-pjsip
```

启用 Linux JRTPLIB adapter：

```bash
cmake -S . -B build-linux-rtp -DGB28181_ENABLE_JRTPLIB=ON
cmake --build build-linux-rtp
```

说明：当前编辑环境不是 Linux；PJSIP/JRTPLIB 的真实链接、启动和抓包验证需要放到 Linux 目标环境执行。当前仓库已有 `3rd/lib/libjthread.a`，但缺少 `3rd/lib/libjrtp.a`，因此 `GB28181_ENABLE_JRTPLIB=ON` 需要先补齐 JRTPLIB 静态库。

运行统一入口：

```bash
./build/gb28181-server -c conf/gb28181-server.conf
```

历史服务目录仍可作为迁移对照目标配置，但不作为默认交付目标。第三方静态库以 Linux 目标为主，例如 `x86_64-unknown-linux-gnu`，因此历史目标建议在 Linux 环境验证。

历史目标配置示例：

```bash
cmake -S . -B build-legacy -DGB28181_BUILD_LEGACY_SERVICES=ON
```

注意：当前配置文件和日志路径存在 `/home/GB28181-Server/...` 这类硬编码路径，阶段 0 后续会整理为统一配置。

## 配置现状

历史配置源仍分散在两个旧目录中，当前已新增 `conf/gb28181-server.conf` 作为统一入口的过渡样例。最终统一配置应描述节点自身、级联对端、SIP 监听、媒体端口和各能力所需参数，而不是按上下级拆成两个服务模块。

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
./gb28181-server -c conf/gb28181-server.conf
```

统一进程启动后初始化 `GB28181Node`，再装配注册客户端、注册服务端、心跳、目录、INVITE、媒体接收、媒体发送等能力模块。`SipSupService` 和 `SipSubService` 仅作为迁移前的代码来源，后续完成整合后以 `gb28181-server` 为唯一工程入口。
