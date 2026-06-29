# GB28181-Server

`GB28181-Server` 是一个 GB/T 28181 学习与流媒体工程化实践项目。当前仓库包含两个历史服务：

- `SipSupService`：上级平台历史目录，负责接收下级注册、处理心跳、发起目录查询和实时预览 INVITE，并接收 RTP/PS 媒体流。
- `SipSubService`：下级平台/设备模拟历史目录，负责向上级注册、发送心跳、响应目录/录像查询、处理 INVITE，并从本地 H.264 测试流封装 PS/RTP 后发送。

阶段 0 的工程目标是把这两个历史服务逐步整理为一个统一项目，最终形成单一服务入口 `gb28181-server`。标准形态是**同一个进程内的 GB28181 节点同时具备注册客户端、注册服务端、心跳、目录、INVITE、媒体收发等能力**。上级/下级是某条级联关系中的相对身份，不应成为工程拆分边界。

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
- [里程碑 3 公共模块盘点](docs/milestone3-common-inventory.md)
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

## 计划中的统一入口

阶段 0 后续目标：

```bash
./gb28181-server -c conf/gb28181-server.conf
```

统一进程启动后初始化 `GB28181Node`，再装配注册客户端、注册服务端、心跳、目录、INVITE、媒体接收、媒体发送等能力模块。`SipSupService` 和 `SipSubService` 仅作为迁移前的代码来源，后续完成整合后以 `gb28181-server` 为唯一工程入口。
