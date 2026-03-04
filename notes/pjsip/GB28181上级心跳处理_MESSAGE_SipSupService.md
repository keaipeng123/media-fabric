# GB28181 下级心跳处理（Keepalive MESSAGE）学习笔记（围绕 `SipSupService` 的 `SipHeartBeat`）

> 对应工程代码（上级侧 / SipSupService）：
>
> - `SipSupService/src/SipCore.cpp`：接收回调入口与任务分发（`onRxRequest()`）
> - `SipSupService/src/SipHeartBeat.cpp`：心跳 MESSAGE 的具体处理（`SipHeartBeat::HeartBeatMessage()`）
> - `SipSupService/src/SipTaskBase.cpp`：通用解析（`parseFromId()` / `parseXmlData()`）
> - `SipSupService/include/SipDef.h`：常量（`SIP_NOTIFY` / `SIP_HEARTBEAT`）
> - `SipSupService/include/GlobalCtl.h`：下级平台列表与状态字段（`SubDomainInfo::registered/lastRegTime`）

本文目标：只讲清 **“上级服务收到下级的 keepalive（SIP MESSAGE + XML body）后，是怎么识别、怎么改状态、怎么回 200/403 的”**。

---

## 1. 一句话概览（你要先抓住的主线）

上级侧处理链路可以浓缩成：

1) PJSIP 收到请求 → `onRxRequest()` 进入应用层模块
2) 如果是 `PJSIP_OTHER_METHOD`（工程里承载 `MESSAGE`）→ 解析 body XML
3) `rootType == "Notify" && CmdType == "keepalive"` → 分发到 `SipHeartBeat`
4) `SipHeartBeat` 根据 `From` 提取下级 ID → 找到该下级节点 → 更新 `lastRegTime`
5) 找到了且该下级 `registered==true` → 回 `200`；否则回 `403`

`lastRegTime` 的更新会影响后续“下级是否掉线”的判断（见 `SipRegister.cpp` 中用 `regTime - lastRegTime >= expires` 来置 `registered=false`）。

---

## 2. 接收入口：`SipCore::onRxRequest()` 如何把心跳分发出去

位置：`SipSupService/src/SipCore.cpp` 的 `onRxRequest(pjsip_rx_data* rdata)`。

关键步骤：

### 2.1 先把 `rdata` clone 一份，交给业务线程

- `pjsip_rx_data_clone(rdata, 0, &param->data);`
- 这样业务线程处理的是 clone 后的数据，避免直接用 PJSIP 内部缓冲导致生命周期问题。

### 2.2 用 method + XML(`CmdType`) 来识别是否为心跳

工程里对请求的识别主要分两条：

- `REGISTER`：`msg->line.req.method.id == PJSIP_REGISTER_METHOD` → `SipRegister`
- 其它（`PJSIP_OTHER_METHOD`）：走 XML 解析，再决定具体任务类

心跳识别逻辑是：

- `rootType`：XML 根节点名（例如 `Notify`）
- `cmdValue`：XML 中 `CmdType` 标签的文本（例如 `keepalive`）

代码等价于：

```cpp
string rootType = "", cmdType = "CmdType", cmdValue;
SipTaskBase::parseXmlData(msg, rootType, cmdType, cmdValue);

if (rootType == SIP_NOTIFY && cmdValue == SIP_HEARTBEAT) {
    param->base = new SipHeartBeat();
}
```

对应常量来自：

- `SIP_NOTIFY` → `"Notify"`
- `SIP_HEARTBEAT` → `"keepalive"`

### 2.3 业务在线程里执行：`dealTaskThread()` + 线程注册

`onRxRequest()` 最后会创建线程执行 `SipCore::dealTaskThread(param)`。

你工程里专门做了一步“PJSIP 线程注册”（非常关键）：

- `pjcall_thread_register(desc);`

原因（直观理解）：

- PJSIP 内部自己创建的线程是“已注册线程”（PJLIB TLS 初始化过）
- 你自己创建的业务线程（`ECThread::createThread`）默认是“外部线程”，直接调用 PJSIP API 可能触发断言：
  - `Assertion failed: !"Calling PJLIB from unknown/external thread"`

因此：所有在业务线程里会调用 PJSIP API 的逻辑（包括 `pjsip_endpt_respond`）都必须先注册线程。

---

## 3. XML 解析：`SipTaskBase::parseXmlData()` 返回了什么

位置：`SipSupService/src/SipTaskBase.cpp`。

该函数做了这些事：

1) `tinyxml2::XMLDocument` 解析 `msg->body->data`
2) `rootType = RootElement()->Value()`（根节点名字）
3) 找 `RootElement()->FirstChildElement(xmlkey)`（这里 xmlkey 传的是 `"CmdType"`）
4) `xmlvalue = element->GetText()`

对心跳来说，你真正用到的只有两件事：

- `rootType`：是否为 `Notify`
- `xmlvalue`：是否为 `keepalive`

### 3.1 一个必须知道的坑：`parseXmlData()` 当前会内存泄漏

`parseXmlData()` 里 `new tinyxml2::XMLDocument()` 后没有 `delete`，属于明显泄漏。

学习/排障阶段不影响你理解链路，但如果心跳频繁，会导致进程内存长期增长。后续如果你要我顺手把这个点工程化修掉，我可以再开一刀做成“不会返回悬空指针、也不会泄漏”的版本。

---

## 4. 心跳处理：`SipHeartBeat::HeartBeatMessage()` 做了什么

位置：`SipSupService/src/SipHeartBeat.cpp`。

### 4.1 下级 ID 从哪里来：`parseFromId()`

`fromId = parseFromId(msg);`

当前实现是：

- 把 `From` 头打印成字符串
- 固定截取：`substr(11, 20)`

它依赖报文形态类似：

```
From: <sip:130909113319427420@x.x.x.x:port>;tag=...
```

所以能得到 20 位的下级平台 ID。

风险点：只要 `From` 头格式/ID 长度变化，这种“硬编码截取”就可能取错。

### 4.2 时间戳用的是什么：`sysinfo().uptime`

工程里更新 `lastRegTime` 用的时间来源优先是：

- `sysinfo(&info)` 成功 → `regTime = info.uptime`（开机到现在的秒数）
- 否则 → `regTime = time(NULL)`（epoch 秒）

这意味着 `lastRegTime` 在大多数 Linux 环境里记录的是 **uptime 秒数**。

这样做的好处是：

- 不受系统时间被手动改动 / NTP 跳变的影响

要点：既然选择 uptime，那么所有比较（例如 `regTime - lastRegTime`）也必须使用同一套时间来源；你的 `SipRegister.cpp` 也是用同样方式取 `regTime`，因此逻辑是自洽的。

### 4.3 核心状态更新逻辑：找到节点 + 已注册才刷新

```cpp
AutoMutexLock lck(&GlobalCtl::globalLock);

auto& lst = GlobalCtl::instance()->getSubDomainInfoList();
auto it = std::find(lst.begin(), lst.end(), fromId);

if (it != lst.end() && it->registered) {
    it->lastRegTime = regTime;
    status_code = 200;
} else {
    status_code = 403;
}
```

含义非常明确：

- 只有该下级已经处于 `registered=true` 时，心跳才“续命”（刷新 `lastRegTime`）
- 否则直接拒绝（403），不更新 `lastRegTime`

这也符合一种常见策略：

- 心跳只用于维持已建立关系
- 未注册的下级不能靠心跳把状态“顶起来”

### 4.4 SIP 层响应：直接回包，不建事务

最后用：

- `pjsip_endpt_respond(..., status_code, ...)`

直接对当前收到的请求 `rdata` 回一个最终响应码。

返回码行为：

- 正常续命：`200`
- 未找到下级 / 未注册：`403`

补充：`SipSupService/include/SipDef.h` 里的 `statusCode::SIP_FORBIDDEN` 注释写的是“Forbidden”，但枚举值是 `404`；而心跳处理代码里直接写的是 `403`。当前以实际回包为准（即心跳返回 403），后续如果要统一枚举/注释，需要单独做一次整理。

---

## 5. 关联：`lastRegTime` 怎么影响“掉线检测”

上级侧对下级是否在线，除了 `registered` 字段外，还依赖心跳刷新 `lastRegTime`。

在 `SipSupService/src/SipRegister.cpp` 的轮询/检查逻辑中（大意）：

- 如果 `regTime - lastRegTime >= expires` → 认为超时 → `registered=false`

因此你可以把 `SipHeartBeat` 看作：

- “给每个已注册下级节点不断续租的触发器”

当心跳不再到达时，`lastRegTime` 不刷新 → 最终会被超时逻辑置为未注册。

