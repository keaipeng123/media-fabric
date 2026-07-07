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
- 通过 `SipTransport` 抽象承载 SIP 收发，默认内存传输用于自测，Linux 可通过 `GB28181_ENABLE_PJSIP` 打开实验性 PJSIP adapter；PJSIP adapter 已覆盖入站 SIP request 和 SIP response 到统一路由的转换。
- 通过 `RtpSessionAdapter` 抽象承载真实 RTP 会话，默认自测使用适配器入口验证 RTP/PS/ES/帧输出和 adapter 发送链路，Linux 可通过 `GB28181_ENABLE_JRTPLIB` 打开实验性 JRTPLIB RTP adapter。
- 通过 `StreamFileFrameSource` 读取历史 `stream.file` 测试流，`MediaSendCapability` 启动时会校验配置媒体源，并按 `[media] stream_send_interval_ms` 周期循环驱动 `sendAnnexBFrame`。
- 能生成 REGISTER、REGISTER Digest 认证重试、MESSAGE keepalive、Catalog、RecordInfo、INVITE 和 INVITE 2xx 后 ACK 出站请求意图。
- 能处理 REGISTER、Keepalive、Catalog、RecordInfo、INVITE、ACK、BYE 入站请求，并生成基础 SIP 响应或业务响应消息；REGISTER 已具备随机 401 Digest challenge、MD5 response 校验、nonce 缓存和 replay 拒绝边界。
- MESSAGE 已具备 MANSCDP XML 解析边界，可提取 root、CmdType、SN、DeviceID、Result、StartTime、EndTime、SumNum、DeviceList/RecordList Item；Catalog Item 当前覆盖 DeviceID、Name、Manufacturer、Model、Owner、CivilCode、Parental、ParentID、SafetyWay、RegisterWay、Secrecy、Status 等常用字段，Record Item 当前覆盖 FilePath、StartTime、EndTime 等常用字段，并校验 Keepalive/Catalog/RecordInfo 的 DeviceID 与列表一致性。
- Catalog/RecordInfo 响应明细已落入节点级业务状态，可查询当前目录项、录像项计数和按 peer 保存的最近一次明细快照，并可导出/恢复文件快照；`BusinessQueryService` 已提供协议无关的 JSON 查询边界，并已接入统一入口的 CLI 管理查询。
- INVITE 已具备轻量 SDP 解析、Call-ID/CSeq/Contact 基础关联、媒体会话记录、SDP 200 响应生成边界，以及收到 INVITE 2xx response 后生成 ACK 出站意图；ACK 会继承 response 的 Call-ID、CSeq、From/To tag，并可优先使用 Contact target。ACK request 会确认媒体会话。发送侧可将 Annex-B 帧封装为 PS payload，并通过 `RtpSessionAdapter::sendPayloadPacket` 切分发送 payload type 96 的 RTP payload packet；接收侧可按 session 更新接收统计和 `stream-receiving` 状态，按 timestamp/marker 重组 RTP payload，解析完整 PS pack/PES，识别视频 PES 中的 Annex-B H.264/H.265 NAL，并通过 `MediaFrameSink` 输出完整 Annex-B 帧到文件。bad SDP 会返回 400，错误 dialog 的 BYE 会返回 481，合法 BYE 会释放本地 RTP 端口。
- 已通过节点级任务注册 REGISTER refresh 和 keepalive 周期保活。
- `--self-test` 可验证 11 条 SIP 路由、REGISTER Digest 认证重试、REGISTER Digest 接受/拒绝、REGISTER nonce replay 拒绝、MESSAGE XML 接受/拒绝、Catalog/RecordInfo 列表响应解析、业务状态快照查询、文件持久化恢复和 JSON 查询、INVITE SIP response 分发、INVITE 2xx 后 ACK 出站、INVITE SDP 接受/拒绝、ACK 确认、历史测试流首帧读取和 PS 回环解析、RTP packetize、RTP 包解析、RTP payload marker 重组、RTP adapter 收包入口、RTP adapter 发送入口、PS/PES 基础解析、H.264/H.265 NAL 识别、Annex-B 帧文件输出和媒体接收状态、peer 注册/心跳状态、RTP 端口分配/释放、出站 SIP 消息计数和周期任务注册。

当前媒体发送配置位于 `conf/gb28181-server.conf` 的 `[media]`：

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

当前已经具备根目录统一 `CMakeLists.txt`，默认构建目标是最终入口 `gb28181-server`。

默认构建：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure -R gb28181-server-self-test
```

默认 CTest 使用 `conf/gb28181-server.conf`；如需指定其它配置，可在 CMake 阶段传入 `-DGB28181_SELF_TEST_CONFIG=path/to/conf`。

里程碑 4 默认构建与自测：

```bash
scripts/verify-milestone4-linux.sh preflight
scripts/verify-milestone4-linux.sh default
```

脚本模式可通过 `GB28181_CONFIG=path/to/conf` 指定自测配置；`preflight` 默认只报告 ready/not-ready，设置 `GB28181_PREFLIGHT_STRICT=1` 后缺关键依赖会返回失败。Linux smoke 启动默认运行 5 秒，可通过 `GB28181_SMOKE_SECONDS=10` 调整。

启用 Linux PJSIP adapter：

```bash
scripts/verify-milestone4-linux.sh pjsip-build
scripts/verify-milestone4-linux.sh pjsip-smoke
```

启用 Linux JRTPLIB adapter：

```bash
scripts/verify-milestone4-linux.sh jrtplib-build
scripts/verify-milestone4-linux.sh jrtplib-smoke
```

同时启用 Linux PJSIP + JRTPLIB adapter，验证最终真实网络组合：

```bash
scripts/verify-milestone4-linux.sh full-build
scripts/verify-milestone4-linux.sh full-smoke
scripts/verify-milestone4-linux.sh full-capture
```

`full-capture` 会把 SIP/RTP pcap 和同时间戳的抓包报告输出到 `artifacts/milestone4/`；tcpdump 异常、pcap 缺失/为空或 pcap 包数为 0 会返回失败。可通过 `GB28181_CAPTURE_SECONDS`、`GB28181_CAPTURE_IFACE`、`GB28181_CAPTURE_DIR` 调整抓包时间、网卡和输出目录。

Linux 环境补齐第三方库后，也可以直接运行里程碑 4 完成门禁：

```bash
scripts/verify-milestone4-linux.sh completion-gate
```

复查已有抓包报告和 pcap 基础证据：

```bash
scripts/verify-milestone4-linux.sh capture-audit artifacts/milestone4
```

查看里程碑 4 抓包过滤器和验收检查点：

```bash
scripts/verify-milestone4-linux.sh capture-help
```

里程碑 4 是否可以标记完成，以 [里程碑 4 完成审计清单](docs/milestone4-completion-audit.md) 为准。

说明：当前编辑环境不是 Linux；PJSIP/JRTPLIB 的真实链接、启动和抓包验证需要放到 Linux 目标环境执行。当前仓库已有 `3rd/lib/libjthread.a`，但缺少 `3rd/lib/libjrtp.a`，因此 `GB28181_ENABLE_JRTPLIB=ON` 需要先补齐 JRTPLIB 静态库。

在 Linux 目标环境已有 `jrtplib-3.11.2` 源码时，可以用脚本补齐：

```bash
scripts/prepare-jrtplib-linux.sh /path/to/jrtplib-3.11.2
```

运行统一入口：

```bash
./build/gb28181-server -c conf/gb28181-server.conf
```

查询已保存的业务状态快照：

```bash
./build/gb28181-server -c conf/gb28181-server.conf --business-query summary --business-state-file state.snapshot
./build/gb28181-server -c conf/gb28181-server.conf --business-query catalog --business-state-file state.snapshot --peer-id 12000000002000000001
./build/gb28181-server -c conf/gb28181-server.conf --business-query record --business-state-file state.snapshot --peer-id 12000000002000000001
```

历史服务目录仍可作为迁移对照目标配置，但不作为默认交付目标。第三方静态库以 Linux 目标为主，例如 `x86_64-unknown-linux-gnu`，因此历史目标建议在 Linux 环境验证。

历史目标配置示例：

```bash
cmake -S . -B build-legacy -DGB28181_BUILD_LEGACY_SERVICES=ON
```

注意：当前配置文件和日志路径存在 `/home/GB28181-Server/...` 这类硬编码路径，阶段 0 后续会整理为统一配置。

## 配置现状

历史配置源仍分散在两个旧目录中，当前已新增 `conf/gb28181-server.conf` 作为统一入口样例。统一配置描述一个本地 SIP 节点和多个 peer 关系：`[node]` 持有唯一的本地 SIP/RTP 配置，`[peer.upstream.*]` 描述需要注册到的远端平台，`[peer.downstream.*]` 描述允许接入或预期接入的下级设备。peer 中的 `remote_port` 是远端端口，不是本地监听端口。

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
./gb28181-server -c conf/gb28181-server.conf
```

统一进程启动后初始化 `GB28181Node`，再装配注册客户端、注册服务端、心跳、目录、INVITE、媒体接收、媒体发送等能力模块。`SipSupService` 和 `SipSubService` 仅作为迁移前的代码来源，后续完成整合后以 `gb28181-server` 为唯一工程入口。
