# GB28181 心跳（Keepalive MESSAGE）学习笔记（围绕 `SipHeartBeat`）

> 对应工程代码：
>
> - [SipSubService/include/SipHeartBeat.h](../../SipSubService/include/SipHeartBeat.h)
> - [SipSubService/src/SipHeartBeat.cpp](../../SipSubService/src/SipHeartBeat.cpp)
> - [SipSubService/include/SipMessage.h](../../SipSubService/include/SipMessage.h)
> - [SipSubService/src/SipMessage.cpp](../../SipSubService/src/SipMessage.cpp)
>
> 关联阅读：REGISTER 的上下级对照见 [GB28181注册_REGISTER_SipRegister.md](GB28181%E6%B3%A8%E5%86%8C_REGISTER_SipRegister.md)

本文目标：只讲清当前工程里 SUB 侧心跳的真实发送方式、报文字段来源、回调如何改状态，以及你在阅读时遇到的 `this`/`.c_str()`/`(char*)` 强转疑问。

---

## 1. 功能定位：心跳做什么、影响什么状态

当前实现位于 SUB 侧（SipSubService），`SipHeartBeat` 做两件事：

1. **定时向所有已注册的上级平台发送心跳**（SIP `MESSAGE`，body 是 GB28181 的 `<Notify><CmdType>keepalive</CmdType>...` XML）。
2. **根据响应结果更新本地状态**：只要事务最终状态码不是 200，就把对应上级节点的 `registered=false`（认为掉线/不可用）。

对应代码在 [SipSubService/src/SipHeartBeat.cpp](../../SipSubService/src/SipHeartBeat.cpp)。

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

这里的关键点：**定时器框架（`TaskTimer`）只认识“函数指针 + 一个 `void*` 参数”这种 C 风格回调**。

因此：

- 回调触发时，框架只能把当初保存的那个 `void*` 原样传回来。
- 回调函数本身是 `static`，没有隐式 `this`。

### 2.2 `static` 成员函数没有隐式 `this`

在 C++ 里：

- 普通成员函数隐式形态是 `void f(SipHeartBeat* this, ...)`，所以能直接用 `this`。
- `static` 成员函数本质上就是“放在类命名空间里的普通函数”，没有 `this`，所以**无法“直接获取到当前对象”**。

所以当前代码在回调里做了标准的 C 回调“回传上下文”写法：

```cpp
SipHeartBeat* pthis=(SipHeartBeat*)param;
...
pthis->gbHeartBeat(*iter);
```

这就是为什么必须把 `this` 作为 `void*` 传进去。

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
