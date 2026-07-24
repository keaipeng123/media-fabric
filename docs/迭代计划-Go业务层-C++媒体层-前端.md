# media-fabric 迭代计划：Go 业务层、C++ 媒体层与前端

## 目标架构

最终交付一个名为 `media-fabric` 的可执行程序和一个进程。Go 负责业务与对外接口，C++ 负责协议与高频媒体处理。

```text
media-fabric（单进程、单二进制）
├── Go 业务层
│   ├── 登录、用户、角色与权限
│   ├── REST API / WebSocket
│   ├── 数据库、审计与配置管理
│   ├── 节点、设备、目录、媒体流业务编排
│   └── 前端静态资源托管
│
└── C++ 媒体层
    ├── GB28181 / SIP / RTP / PS
    ├── 注册、心跳、目录、INVITE、BYE
    ├── 媒体会话、端口与流统计
    ├── 解复用、转码与 FLV 封装
    └── 后续 LiveKit RTP 输入适配
```

前端独立开发、独立构建；生产环境由 Go 服务托管构建产物，用户只需部署和启动 `media-fabric`。

## 迭代 1：C++ 媒体层库化

状态：已完成。

### 目标

保持当前 GB28181 功能不变，将 C++ 工程从仅可执行程序重构为可被 Go 嵌入的媒体内核。

### 工作内容

- 将现有启动逻辑从 `main.cpp` 拆出。
- CMake 新增 `media_fabric_core` 静态库目标。
- 保留当前 C++ 独立入口，用于 SIP/RTP 协议调试和回归验证。
- 为媒体核心设计稳定的 C ABI，覆盖：
  - 初始化、启动、停止、销毁；
  - 节点列表与注册状态；
  - 目录查询；
  - 按目录设备 ID 发起 INVITE 和 BYE；
  - 流状态、RTP 统计和日志事件；
  - 后续的媒体数据输入、FLV 数据输出。

### 验收

- `mfcli` 现有命令保持可用。
- 上下级注册、心跳、目录查询、按目录设备 INVITE、RTP 收流均不回归。
- C++ 独立入口与静态库使用同一套核心实现。

## 迭代 2：Go 宿主程序

状态：已完成基础宿主、CGO 生命周期桥接与单二进制构建流程；HTTP 业务 API 留待迭代 3。

### 目标

由 Go 成为正式服务入口，使用 CGO 链接 C++ 静态媒体库，输出最终的 `media-fabric` 二进制。

### 建议目录

```text
go/
├── cmd/media-fabric/       # 最终 Go main
├── internal/api/           # HTTP / WebSocket
├── internal/auth/          # 认证与授权
├── internal/config/        # Go 侧配置
├── internal/media/         # C++ C ABI 封装
├── internal/store/         # 数据存储
└── internal/user/          # 用户业务
```

### 工作内容

- Go 统一处理配置加载、日志初始化、信号退出和进程生命周期。
- CGO 封装 C++ C ABI，不暴露 C++ 类、异常或 STL 容器。
- Go 调用 C++ 启动 GB28181/SIP/RTP 媒体核心。
- 暂时保留现有 Unix Socket 管理接口，保证 `mfcli` 兼容；后续由 Go API 逐步替代。

### 验收

- `./media-fabric -c conf/media-fabric.conf` 启动 Go 宿主和 C++ 媒体层。
- 一个进程内完成原有 SIP、RTP 和媒体能力。
- 正常退出时，Go 与 C++ 资源均可有序释放。

## 迭代 3：业务基础能力

状态：已完成认证、用户管理、RBAC、PostgreSQL 持久化与审计基础；前端接入留待迭代 4。

### 目标

提供前端可使用的登录、用户、角色和权限能力。

### 工作内容

- 使用 PostgreSQL 保存用户、角色、审计记录和业务配置。
- 实现账号密码登录、JWT access token 与 refresh token。
- 初期 RBAC 角色：
  - `admin`：系统和用户管理；
  - `operator`：节点、设备和开流操作；
  - `viewer`：只读查看。
- 记录登录、用户变更、开流、停流和配置修改等审计日志。
- 以 OpenAPI 定义前后端接口契约。

### 首批接口

```text
POST   /api/v1/auth/login
POST   /api/v1/auth/refresh
GET    /api/v1/me
GET    /api/v1/users
POST   /api/v1/users
PATCH  /api/v1/users/{id}
```

### 验收

- 管理员可完成登录、创建用户、禁用用户和重置密码。
- 无权限请求不能访问用户及媒体管理接口。
- 关键操作存在可查询的审计记录。

## 迭代 4：管理前端

### 目标

完成可用于日常管理的 Web 控制台。

### 推荐技术栈

- Vue 3
- TypeScript
- Vite
- Element Plus
- Pinia

### 建议目录

```text
web/
├── src/router/
├── src/services/
├── src/stores/
└── src/views/
    ├── Login/
    ├── Dashboard/
    ├── Users/
    └── Profile/
```

### 首批页面

- 登录页；
- 首页仪表盘；
- 用户管理；
- 个人中心；
- 角色与权限管理；
- 系统运行状态。

### 验收

- 管理员可通过浏览器完成用户管理。
- 前端路由和按钮均受权限控制。
- 首页可展示服务、节点和媒体层基础运行状态。

## 迭代 5：GB28181 管理界面

### 目标

将当前 `mfcli` 的调试能力逐步产品化为 Web 管理能力。

### 页面与能力

- 节点管理：上级、下级、地址、注册状态、最后心跳时间；
- 设备目录：按下级平台查看同步目录；
- 设备详情：设备信息、在线状态、所属平台；
- 开流管理：按目录设备 ID 发起 INVITE、发送 BYE；
- 流状态：Call-ID、RTP 地址、收发包、帧数、字节数和错误信息；
- SIP 与媒体日志查询。

### API

```text
GET    /api/v1/nodes
GET    /api/v1/catalog/devices
POST   /api/v1/streams
GET    /api/v1/streams
DELETE /api/v1/streams/{deviceId}
```

### 验收

- 前端可查看节点注册状态和设备目录。
- 前端可按目录设备 ID 开流和停流。
- 媒体流状态与 C++ 层统计一致。

## 迭代 6：LiveKit 与 HTTP-FLV

### 目标

将 LiveKit 房间内的指定参会者媒体流转化为 `media-fabric` 自身提供的 HTTP-FLV。

### 工作内容

- Go 负责加入 LiveKit 房间、选择参会者和订阅音视频轨。
- Go 将 RTP 数据交给 C++ 媒体层。
- C++ 完成 RTP 解包、必要转码和 FLV 封装。
- Go 内置 HTTP-FLV 分发、播放鉴权和流生命周期管理。
- 前端增加 LiveKit 房间、参会者、转流任务和播放器页面。

### 注意事项

- LiveKit 常见音频编码为 Opus，传统 FLV 一般需要 AAC 或 MP3；音频通常需要转码。
- FLV 播放兼容性优先使用 H.264 视频；VP8、VP9、AV1 或 H.265 可能需要转为 H.264。
- 新播放客户端只能从关键帧开始接收，需要缓存 FLV Header、音视频配置和最近关键帧。

### 验收

- 可选择房间中的指定参会者创建转流任务。
- 前端可拿到受鉴权保护的 HTTP-FLV 地址并播放。
- 参会者离开或停止任务后，媒体会话和 HTTP 客户端可正确关闭。

## 迭代 7：统一构建与交付

### 目标

统一构建 C++、Go 与前端，最终只交付 `media-fabric` 服务。

### 构建流程

```text
CMake 编译 C++ media_fabric_core 静态库
  -> Go CGO 链接媒体库并输出 media-fabric
  -> 构建 web/dist
  -> 将前端静态资源嵌入或由 Go 托管
```

### 部署目标

```text
media-fabric
conf/media-fabric.conf
数据库连接配置
可选：TLS 证书、媒体文件目录
```

启动命令：

```bash
./media-fabric -c conf/media-fabric.conf
```

## 跨层约束

- C++ 不直接访问数据库，不处理 JWT，也不定义 HTTP 路由。
- Go 不直接实现 SIP、RTP、PS 等高频协议细节。
- CGO 接口只传递简单结构、字节数组和句柄；不得跨边界传递 C++ 对象、异常和 STL 容器。
- C++ 不能长期保存 Go 指针；跨层事件应使用线程安全队列、复制数据或受控回调。
- LiveKit 的 RTP 数据进入 C++ 时，应采用批量缓冲或环形队列，避免每个 RTP 包产生高成本跨语言调用。
- 每个迭代均保留上下级注册、目录查询、INVITE、RTP 收流的集成回归测试。
