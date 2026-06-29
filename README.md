# GB28181-Server

`GB28181-Server` 是一个 GB/T 28181 学习与流媒体工程化实践项目。当前仓库包含两个历史服务：

- `SipSupService`：上级平台角色，负责接收下级注册、处理心跳、发起目录查询和实时预览 INVITE，并接收 RTP/PS 媒体流。
- `SipSubService`：下级平台/设备模拟角色，负责向上级注册、发送心跳、响应目录/录像查询、处理 INVITE，并从本地 H.264 测试流封装 PS/RTP 后发送。

阶段 0 的工程目标是把这两个历史服务逐步整理为一个统一项目，最终形成单一服务入口 `gb28181-server`，运行时通过 `sup`、`sub` 或 `both` 角色启用对应能力。

## 当前能力

- GB/T 28181 REGISTER 注册与 Digest 鉴权。
- MESSAGE keepalive 心跳。
- Catalog 目录查询与响应。
- RecordInfo 录像查询基础流程。
- INVITE/BYE 实时预览与停止。
- RTP/RTCP 会话创建，支持 UDP 与部分 TCP 主被动连接逻辑。
- PS mux/demux。
- H.264 裸流读取、PS 封装、RTP 发送。
- RTP/PS 接收、组包、PS 解封装、H.264 裸流提取。

## 文档入口

- [项目式学习计划](docs/流媒体开发项目式学习计划.md)
- [阶段 0 执行计划](docs/阶段0-整理现有项目基础执行计划.md)
- [架构现状](docs/architecture.md)
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

## 构建现状

当前还没有根目录统一 `CMakeLists.txt`，两个历史服务分别通过各自 `cmake/CMakeLists.txt` 构建。第三方静态库以 Linux 目标为主，例如 `x86_64-unknown-linux-gnu`，因此建议先在 Linux 环境验证。

示例：

```bash
cmake -S SipSupService/cmake -B build/SipSupService
cmake --build build/SipSupService

cmake -S SipSubService/cmake -B build/SipSubService
cmake --build build/SipSubService
```

注意：当前配置文件和日志路径存在 `/home/GB28181-Server/...` 这类硬编码路径，阶段 0 后续会整理为统一配置。

## 运行配置

上级服务配置：

- `SipSupService/conf/SipSupService.conf`
- 默认 SIP ID：`10000000002000000001`
- 默认 SIP 端口：`5061`
- 默认 RTP 端口范围：`20000-30000`

下级服务配置：

- `SipSubService/conf/SipSubService.conf`
- 默认 SIP ID：`11000000002000000001`
- 默认 SIP 端口：`7101`
- 默认 RTP 端口范围：`30000-40000`

测试媒体与目录数据：

- `SipSubService/conf/stream.file`
- `SipSubService/conf/catalog.json`

## 计划中的统一入口

阶段 0 后续目标：

```bash
./gb28181-server --role sup
./gb28181-server --role sub
./gb28181-server --role both
```

当前仍保留 `SipSupService` 和 `SipSubService` 两个历史入口，用于对照验证。
