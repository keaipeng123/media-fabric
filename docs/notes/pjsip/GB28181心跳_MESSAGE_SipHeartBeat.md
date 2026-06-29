# GB28181 心跳（Keepalive MESSAGE）学习笔记（围绕 `SipHeartBeat`）

> 对应工程代码：
>
> - [SipSubService/include/SipHeartBeat.h](../../../SipSubService/include/SipHeartBeat.h)
> - [SipSubService/src/SipHeartBeat.cpp](../../../SipSubService/src/SipHeartBeat.cpp)
> - [SipSubService/include/SipMessage.h](../../../SipSubService/include/SipMessage.h)
> - [SipSubService/src/SipMessage.cpp](../../../SipSubService/src/SipMessage.cpp)
>
> 关联阅读：REGISTER 的上下级对照见 [GB28181注册_REGISTER_SipRegister.md](GB28181%E6%B3%A8%E5%86%8C_REGISTER_SipRegister.md)

本文目标：只讲清当前工程里 SUB 侧心跳的真实发送方式、报文字段来源、回调如何改状态，以及你在阅读时遇到的 `this`/`.c_str()`/`(char*)` 强转疑问。

---

## 1. 功能定位：心跳做什么、影响什么状态

当前实现位于 SUB 侧（SipSubService），`SipHeartBeat` 做两件事：

1. **定时向所有已注册的上级平台发送心跳**（SIP `MESSAGE`，body 是 GB28181 的 `<Notify><CmdType>keepalive</CmdType>...` XML）。
2. **根据响应结果更新本地状态**：只要事务最终状态码不是 200，就把对应上级节点的 `registered=false`（认为掉线/不可用）。

对应代码在 [SipSubService/src/SipHeartBeat.cpp](../../../SipSubService/src/SipHeartBeat.cpp)。

---

## 2. 定时器触发链路：为什么 `HeartBeatProc` 要传 `this`

### 2.1 `TaskTimer` 回调签名决定了必须传入上下文指针

`SipHeartBeat::gbHeartBeatServiceStart()`：

- `m_heartTimer->setTimerFun(HeartBeatProc, this);`
- `m_heartTimer->start();`

`HeartBeatProc` 是：

```cpp
static void HeartBeatProc(void* param);
```

这里“签名决定必须传入上下文指针”可以理解成一句大白话：

> `TaskTimer` 只会在时间到了之后调用 `fun(param)`，它不会（也没法）帮你自动找到“应该操作哪个对象”。

也就是说，`TaskTimer` 的能力非常固定：

- 你给它一个**函数指针**（比如 `HeartBeatProc`）
- 再给它一个**额外参数**（类型被统一写成 `void*`，能装下“任何指针”）
- 之后它在内部线程里，按周期去做：`m_timerFun(m_funParam)`

结合工程实现看会更直观（见 `SipSubService/src/TaskTimer.cpp`）：

1. `setTimerFun(fun, param)`：把回调函数保存到 `m_timerFun`，把参数保存到 `m_funParam`
2. `start()`：创建线程，线程入口是 `TaskTimer::timer(void* context)`，并把 **`TaskTimer` 自己的 `this`** 作为 `context` 传进去
3. 在线程函数 `timer()` 里，每到时间点就执行：

    ```cpp
    pthis->m_timerFun(pthis->m_funParam);
    ```

注意第 3 步：**`TaskTimer` 调回调时，唯一能传回去的“业务信息”就是那个 `void* m_funParam`**。所以你想在回调里操作某个对象，就必须提前把“对象地址”塞进这个 `void*`。

因此在 `SipHeartBeat::gbHeartBeatServiceStart()` 里才会写：

- `m_heartTimer->setTimerFun(HeartBeatProc, this);`

含义就是：

- “将来定时器触发时，请调用 `HeartBeatProc(param)`”
- “这里的 `param` 就用当前这个 `SipHeartBeat` 对象的地址（`this`）”

### 2.2 `static` 成员函数没有隐式 `this`

在 C++ 里要分清两种函数：

- **普通成员函数**：调用时必须“绑定某个对象”，它天然就知道自己属于谁，所以函数体里可以直接用 `this`。
    - 可以粗略理解成编译器会帮你偷偷多传一个参数：`SipHeartBeat* this`
- **`static` 成员函数**：它不属于某个具体对象，更像“放在类作用域里的普通函数”，所以它**没有**隐式 `this`。

但 `TaskTimer` 需要的是“可直接用函数指针调用的回调”，所以回调通常写成 `static`（或者干脆写成普通全局函数）。既然回调拿不到 `this`，那就只能靠 `void* param` 把对象地址带回来。

所以你看到的就是非常经典的 C 风格回调写法（把上下文指针强转回真实类型）：

```cpp
SipHeartBeat* pthis=(SipHeartBeat*)param;
...
pthis->gbHeartBeat(*iter);
```

这就是为什么必须把 `this` 作为 `void*` 传进去。

### 2.3 一个“闹钟”类比（帮助快速理解）

把 `TaskTimer` 想象成闹钟：

- 你只能告诉闹钟两件事：
    1) 时间到了要“拨打哪个号码”（回调函数指针）
    2) 拨通后要“报什么暗号”（`void* param`）

`this` 就是那个暗号：闹钟响的时候，回调函数通过 `param` 才知道“我应该去操作哪个 `SipHeartBeat` 对象”。

---

## 3. 发送心跳的报文是怎么拼出来的

`SipHeartBeat::gbHeartBeat(GlobalCtl::SupDomainInfo& node)`：

### 3.1 请求方法：使用 PJSIP 自定义方法发送 `MESSAGE`

```cpp
string method="MESSAGE";
pjsip_method reqMethod={PJSIP_OTHER_METHOD,{(char*)method.c_str(),method.length()}};
```

这里构造了一个 `pjsip_method`，类型是 `PJSIP_OTHER_METHOD`，并把名字填成 `MESSAGE`。

### 3.2 Request-URI / From / To 的来源

先用 `SipMessage` 生成 3 个字符串，再包成 `pj_str_t`：

- `From`: `msg.setFrom(本端sipId, 本端sipIp)` → `"<sip:...@...>"`
- `To`: `msg.setTo(对端sipId, 对端addrIp)` → `"<sip:...@...>"`
- `Request-URI`: `msg.setUrl(对端sipId, 对端addrIp, 对端sipPort)` → `"sip:...@...:...;transport=udp"`

然后：

```cpp
pj_str_t from =pj_str(msg.FromHeader());
pj_str_t to=pj_str(msg.ToHeader());
pj_str_t line=pj_str(msg.RequestUrl());
```

最后调用：

```cpp
pjsip_endpt_create_request(endpt, &reqMethod, &line, &from, &to, ... , &tdata);
```

### 3.3 Body：`Application/MANSCDP+xml` + XML 文本

body 部分：

```cpp
pj_str_t type =pj_str("Application");
pj_str_t subtype=pj_str("MANSCDP+xml");
pj_str_t xmldata=pj_str((char*)keepalive.c_str());
tdata->msg->body=pjsip_msg_body_create(tdata->pool,&type,&subtype,&xmldata);
```

其中 `keepalive` 是用 `std::string` 拼出来的 XML 字符串。

---

## 4. 响应回调：为什么 token 传 `&node`，以及状态怎么改

发送时：

```cpp
status=pjsip_endpt_send_request(endpt, tdata, -1, &node, &response_callback);
```

回调签名：

```cpp
static void response_callback(void *token, pjsip_event *e)
```

`token` 会原样回传，所以回调里：

```cpp
GlobalCtl::SupDomainInfo* node=(GlobalCtl::SupDomainInfo*)token;
if(tsx->status_code!=200) {
    node->registered=false;
}
```

这表示：对端只要没回 200，本地就认为与该上级的“已注册态”失效。

---

## 5. `.c_str()` 与 `(char*)` 强转：逐个问题对齐解释

这一节专门回答你问到的几行“为什么这样写”。

### 5.1 为什么 `std::string` 要 `.c_str()` 才能给 PJSIP 用？

PJSIP/PJLIB 大量接口是 C API，使用 C 字符串：`char*` 或 `const char*`。

而 `std::string` 是 C++ 对象，不能隐式当做 `char*` 传入。

因此需要：

- `keepalive.c_str()`：把 `std::string` 的内容以 **NUL 结尾** 的 C 字符串指针形式取出来。

要点：`.c_str()` 返回的是 `const char*`，并且只要 `std::string` 不被修改/析构，这个指针就有效。

### 5.2 为什么经常还要写 `(char*)xxx.c_str()` 这种强转？

这是因为 PJSIP 的一些宏/函数（例如 `pj_str()`) 在接口层面使用的是 `char*`（非 const），但你手里的是 `const char*`：

- `std::string::c_str()` → `const char*`
- 但 `pj_str()` 常见原型/宏形态是“接收 `char*`”

于是写成 `(char*)keepalive.c_str()` 是为了**让编译器不报类型不匹配**。

严格讲：这是“去掉 const 限定”的写法（const cast），本质上依赖一个事实：PJSIP 并不会去修改你传入的那块只读字符串内容。

### 5.3 `pj_str("Application")` 为什么不需要 `.c_str()`？

因为 `"Application"` 本身就是 C 字符串字面量，不是 `std::string`，所以没有 `.c_str()` 这一说。

但在 C++ 里，字符串字面量类型是 `const char[N]`，按理说传给需要 `char*` 的接口仍然会有“丢弃 const”的问题。

你现在这行之所以“看起来不需要强转”，通常是以下情况之一：

- `pj_str()` 在你当前编译环境下被定义为能接受 `const char*`（或内部做了转换）。
- 或者编译器只是给 warning，但允许通过。

### 5.4 `pj_str((char*)keepalive.c_str())` 为什么又需要强转？

因为这里是 `std::string` → `.c_str()`，得到的是 `const char*`，而 `pj_str()` 可能期望 `char*`，所以你看到的代码用 `(char*)` 来消除类型不匹配。

### 5.5 `(char*)node.sipId.c_str()` 为什么也需要强转？

原因同上：

- `node.sipId` 是 `std::string`
- `.c_str()` 是 `const char*`
- `SipMessage::setTo/setUrl` 的形参是 `char*`（不是 `const char*`）

所以必须强转才能调用当前的 `SipMessage` 接口。

如果 `SipMessage` 当初把形参写成 `const char*`，这里就不需要强转了（但这是代码风格/接口 const-correctness 的问题，不影响你理解当前行为）。
