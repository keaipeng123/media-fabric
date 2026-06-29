# 里程碑 3：公共模块盘点

更新日期：2026-06-29

## 目标

将 `SipSupService` 与 `SipSubService` 中和上下级身份无关的基础设施代码收敛到 `gb28181_common` 静态库，为后续 `GB28181Node` 和能力模块复用做准备。

本阶段不抽取 REGISTER、Catalog、INVITE、RTP/PS 等业务链路代码。

## 第一批抽取结果

第一批只抽取头文件和源文件在两个历史目录中完全一致、且不依赖历史业务全局对象的模块。

```text
src/common/
  include/
    ConfReader.h
    ECEventPoll.h
    ECSocket.h
    ECThread.h
    SipDef.h
    SipMessage.h
    ThreadPool.h
    XmlParser.h
    AutoMutexLock.h
    JsonParse.h
    StreamHeader.h
  src/
    ConfReader.cpp
    ECEventPoll.cpp
    ECSocket.cpp
    ECThread.cpp
    SipMessage.cpp
    ThreadPool.cpp
    XmlParser.cpp
```

对应 CMake 目标：

```text
gb28181_common
```

当前 `gb28181-server` 已链接 `gb28181_common`。

## 模块判断

| 模块 | 头文件是否一致 | 源文件是否一致 | 当前处理 | 说明 |
| --- | --- | --- | --- | --- |
| `ConfReader` | 是 | 是 | 已抽取 | 配置读取工具，不依赖上下级身份。 |
| `ECThread` | 是 | 是 | 已抽取 | pthread 轻封装；`sys/prctl.h` 仅在 Linux 下包含，避免非 Linux 编辑环境做基础检查时卡在平台头文件。 |
| `XmlParser` | 是 | 是 | 已抽取 | tinyxml2 轻封装，供后续目录、响应 XML 生成复用。 |
| `TaskTimer` | 是 | 是 | 推迟到里程碑 4 | 代码依赖 `GlobalCtl`、glog、PJSIP 线程注册，应随 capability 生命周期设计处理。 |
| `SipMessage` | 是 | 是 | 已抽取 | 公共版去掉 `Common.h`、`GlobalCtl.h` 依赖，并将 `sprintf` 改为 `snprintf`。 |
| `ECSocket` | 是 | 否 | 已抽取 | 源文件差异主要是注释和 `select/epoll` 选择；公共版保留 Linux 优先 epoll。 |
| `ECEventPoll` | 否 | 否 | 已抽取 | 头源差异主要是注释；公共版保留 select 与 Linux epoll 两种实现。 |
| `ThreadPool` | 否 | 否 | 已抽取 | 公共版采用通用 `ThreadTask` 模型，去掉 `EventMsgHandle`、`bufferevent`、PJSIP 线程注册和历史全局头依赖。 |
| `SipTaskBase` | 否 | 否 | 推迟到里程碑 4 | 已进入 SIP 业务任务抽象，后续随 capability 设计处理。 |
| `Common` | 否 | 无单独源文件 | 拆分中 | 不整体搬迁；已拆出 `AutoMutexLock`、`JsonParse`、`StreamHeader`。日志路径和服务名宏继续留在历史目录，后续进入统一配置。 |
| `SipDef` | 否 | 无单独源文件 | 已抽取 | 公共版只保留协议常量、状态码和设备类型枚举；上级特有 `DeviceInfo/CommandCode` 不进入 common。 |

## Common.h 拆分原则

`Common.h` 当前混合了多类职责：

- 日志目录和服务名宏：`LOG_DIR`、`LOG_FILE_NAME`
- RAII 锁：`AutoMutexLock`
- JSON 字符串转换：`JsonParse`
- 下级媒体流头：`StreamHeader`

公共库中不保留新的大 `Common.h`。可复用内容按职责拆成小头文件：

```text
AutoMutexLock.h
JsonParse.h
StreamHeader.h
```

暂不迁移的内容：

- `LOG_DIR`
- `LOG_FILE_NAME`
- 上下级不同的服务日志名

这些内容后续应进入统一配置和统一日志初始化，而不是进入公共基础库。

## 验证

已执行：

```bash
cmake -S . -B build
cmake --build build
./build/gb28181-server -c conf/gb28181-server.conf
```

验证结果：

- `gb28181_common` 可独立编译为静态库。
- `gb28181-server` 可链接该静态库。
- 统一入口仍可正常启动 bootstrap。

当前编辑环境构建 `ThreadPool` 时可能出现 unnamed semaphore 的平台警告。目标运行环境是 Linux，该警告不影响 Linux 目标开发判断。

## 里程碑 3 收口判断

里程碑 3 的公共基础设施抽取已完成。当前 common 已覆盖：

- 配置读取
- 线程封装
- 线程池
- socket 建连
- select/epoll 事件等待
- XML 生成
- SIP 基础消息头拼装
- SIP 基础协议常量
- RAII 锁、JSON 转换、媒体测试头结构

不在里程碑 3 继续处理的内容：

- `TaskTimer`：需要结合 PJSIP 线程注册和 capability 生命周期设计。
- `SipTaskBase`：已经属于 SIP 业务任务抽象，应在里程碑 4 设计能力模块接口时处理。
- 上级特有 `DeviceInfo/CommandCode`：属于命令入口和开流会话模型，后续放入节点/能力模块设计。
