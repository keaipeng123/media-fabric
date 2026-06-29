# RTPSession 相关问答整理

本文整理了本次会话中围绕 jrtplib 的 RTPSession、RTP 时间戳单位、90k 时钟以及时间戳增量计算的几个核心问题与解答，便于后续查阅。

结合的代码位置：

- [SipSupService/src/Gb28181Session.cpp](../../../SipSupService/src/Gb28181Session.cpp#L480)
- [3rd/include/jrtplib3/rtpsessionparams.h](../../../3rd/include/jrtplib3/rtpsessionparams.h#L54)
- [3rd/include/jrtplib3/rtpudpv4transmitter.h](../../../3rd/include/jrtplib3/rtpudpv4transmitter.h#L66)
- [3rd/include/jrtplib3/rtpsession.h](../../../3rd/include/jrtplib3/rtpsession.h#L94)

## 1. CreateRtpSession 这段代码在做什么

示例代码：

```cpp
LOG(INFO) << "CreateRtpSession";
RTPSessionParams sessParams;
sessParams.SetOwnTimestampUnit(1.0/90000.0);
sessParams.SetAcceptOwnPackets(true);
sessParams.SetUsePollThread(true);
sessParams.SetNeedThreadSafety(true);
sessParams.SetMinimumRTCPTransmissionInterval(RTPTime(5,0));

int ret = -1;
RTPUDPv4TransmissionParams transparams;
transparams.SetRTPReceiveBuffer(1024*1024);
transparams.SetPortbase(20000);

ret = Create(sessParams, &transparams);
LOG(INFO) << "creat rtp ret:" << ret;
if (ret < 0)
{
	LOG(ERROR) << "udp create fail";
}
else
{
	LOG(INFO) << "udp create ok,bind:" << 20000;
}
```

这段代码的作用是：

1. 创建一个基于 IPv4 UDP 的 RTP 会话。
2. 配置 RTP 会话级参数，比如时间戳单位、线程模型、RTCP 间隔。
3. 配置 UDP 传输参数，比如接收缓冲区、本地绑定端口。
4. 调用 jrtplib 的 Create 完成底层 socket 和会话初始化。

如果创建成功，这个会话就具备了接收 RTP 和 RTCP 数据的基础能力。

## 2. 每个参数是什么意思

### 2.1 RTPSessionParams

#### SetOwnTimestampUnit(1.0/90000.0)

表示 RTP 时间戳的单位是 1/90000 秒。

含义是：

- RTP 头里 timestamp 字段每增加 1，表示媒体时间前进了 1/90000 秒。
- timestamp 增加 90000，表示媒体时间前进了 1 秒。

这通常用于视频流，因为 RTP 视频常用 90000 作为时钟频率。

#### SetAcceptOwnPackets(true)

表示是否接收自己发出的 RTP 包。

常见用途：

- 本机回环调试。
- 单机联调用于观察自己发送的包。

在生产环境中不一定必须开启。

#### SetUsePollThread(true)

表示使用 jrtplib 内部的轮询线程。

开启后，jrtplib 会在后台线程中自动：

- 处理接收到的 RTP/RTCP 数据。
- 按需发送 RTCP 包。

这样业务线程不需要自己手工轮询。

#### SetNeedThreadSafety(true)

表示要求 jrtplib 提供线程安全保护。

因为启用了轮询线程，会话对象可能被多个线程同时访问，所以需要内部互斥保护。

#### SetMinimumRTCPTransmissionInterval(RTPTime(5,0))

表示最小 RTCP 发送间隔为 5 秒。

RTCP 是 RTP 的控制协议，用于：

- 发送统计信息。
- 反馈丢包、抖动等状态。
- 发送 BYE、SDES 等控制消息。

这里设置为 5 秒，表示 RTCP 不会发得过于频繁。

### 2.2 RTPUDPv4TransmissionParams

#### SetRTPReceiveBuffer(1024*1024)

表示将 RTP socket 的接收缓冲区设置为 1 MB。

作用：

- 降低高码流视频下因内核接收缓冲区过小导致的丢包风险。
- 给突发流量和网络抖动留出更大缓存空间。

#### SetPortbase(20000)

表示 RTP 本地端口基址设置为 20000。

在经典 RTP/RTCP 端口对中：

- RTP 端口通常是 20000。
- RTCP 端口通常是 20001。

jrtplib 要求这个端口基址通常为偶数，除非显式允许奇数端口。

### 2.3 Create(sessParams, &transparams)

这一步是真正创建会话。

默认使用的是 IPv4 UDP 传输方式。Create 会做的事情包括：

- 创建 RTP 会话对象。
- 初始化 UDP 传输器。
- 绑定本地 RTP/RTCP 端口。
- 初始化 SSRC、CNAME、RTCP 调度器等内部状态。
- 如果开启了轮询线程，则启动后台处理线程。

## 3. 什么是 RTP 时间戳单位

RTP 时间戳单位，指的是：

RTP 头里 timestamp 字段每增加 1，代表真实时间前进了多少。

它本质上定义的是：

- RTP 时间戳这个数字。
- 真实时间之间的换算关系。

例如，当时钟频率是 90000 时：

```text
1 个 RTP 时间戳单位 = 1/90000 秒
```

也就是：

- timestamp 加 1，时间过去 1/90000 秒。
- timestamp 加 90000，时间过去 1 秒。

## 4. 什么是 90000 时钟

90000 时钟，准确说是：

RTP 的 clock rate 是 90000 Hz。

意思是：

- 每 1 秒，RTP 时间戳走 90000 个单位。

它不是 CPU 时钟，也不是系统时间，而是媒体流自己的时间基准。

可以理解为一把专门给媒体流用的时间尺子：

```text
1 秒 = 90000 个 timestamp 单位
```

所以每个单位表示：

```text
1/90000 秒
```

视频里常用 90000，是因为它适合表示常见帧率，比如 25fps、30fps、50fps、60fps。

## 5. 25fps 时为什么每帧 timestamp 增量是 3600

25fps 表示：

- 每秒 25 帧。

所以每帧时间是：

$$
\frac{1}{25} = 0.04\ \text{秒} = 40\ \text{ms}
$$

如果 RTP 时钟频率是 90000，那么 1 秒对应 90000 个 timestamp 单位。

那么 0.04 秒对应的 timestamp 增量就是：

$$
90000 \times 0.04 = 3600
$$

也可以写成：

$$
\frac{90000}{25} = 3600
$$

所以 25fps 视频里，相邻两帧的 RTP 时间戳通常相差 3600。

举例：

- 第 1 帧 timestamp = 0
- 第 2 帧 timestamp = 3600
- 第 3 帧 timestamp = 7200
- 第 4 帧 timestamp = 10800

重点不是第一帧从多少开始，而是每帧之间增长多少。

## 6. 通用计算公式

### 已知时钟频率，求 1 个 timestamp 单位对应多少秒

如果 RTP 时钟频率为 f，则：

$$
1\ \text{个 RTP 时间戳单位} = \frac{1}{f}\ \text{秒}
$$

### 已知帧率，求每帧 timestamp 增量

如果视频帧率为 fps，RTP 时钟频率为 f，则每帧时间戳增量为：

$$
\frac{f}{fps}
$$

例如：

- 25fps: 90000 / 25 = 3600
- 30fps: 90000 / 30 = 3000
- 50fps: 90000 / 50 = 1800
- 60fps: 90000 / 60 = 1500

### 已知 timestamp 差值，换算真实时间

如果两个 RTP 包的时间戳差值为 delta，时钟频率为 f，则它们之间的真实时间间隔为：

$$
\frac{delta}{f}\ \text{秒}
$$

例如时间戳差值是 3600，时钟频率是 90000：

$$
\frac{3600}{90000} = 0.04\ \text{秒}
$$

## 7. 容易混淆的点

### 7.1 RTP timestamp 不是毫秒

RTP timestamp 一般不是直接用毫秒表示，而是按媒体约定的 clock rate 递增。

例如视频常见用 90000 时钟，所以 timestamp 增量不是 40，而是 3600。

### 7.2 RTP timestamp 不是包序号

RTP 里有两个容易混淆的字段：

- Sequence Number: 每发一个 RTP 包通常加 1，用来检查丢包、乱序。
- Timestamp: 表示媒体播放时间位置，用来恢复播放顺序、做同步和抖动计算。

### 7.3 第一帧时间戳不一定从 0 开始

第一帧的 timestamp 往往可以是随机值。

真正重要的是：

- 相邻帧之间的增量是否正确。
- 整个流的时间轴是否连续。

## 8. 一句话总结

RTP 时间戳单位，就是 RTP 时间戳每增加 1 所代表的真实时间长度。

当 clock rate 为 90000 时：

- 1 个 timestamp 单位 = 1/90000 秒。
- 1 秒 = 90000 个 timestamp 单位。
- 25fps 视频每帧时长 40ms，所以每帧 timestamp 增量通常是 3600。

## 9. OnNewSource、OnRemoveSource、OnBYEPacket 的作用

本节整理的问题是：

> 结合 rtpsession 头文件解释下 OnNewSource，OnRemoveSource，OnBYEPacket 函数的作用。

相关代码位置：

- [3rd/include/jrtplib3/rtpsession.h](../../../3rd/include/jrtplib3/rtpsession.h#L492)
- [3rd/include/jrtplib3/rtpsession.h](../../../3rd/include/jrtplib3/rtpsession.h#L495)
- [3rd/include/jrtplib3/rtpsession.h](../../../3rd/include/jrtplib3/rtpsession.h#L536)
- [SipSupService/include/Gb28181Session.h](../../../SipSupService/include/Gb28181Session.h#L105)
- [SipSupService/include/Gb28181Session.h](../../../SipSupService/include/Gb28181Session.h#L140)
- [SipSupService/include/Gb28181Session.h](../../../SipSupService/include/Gb28181Session.h#L174)

### 9.1 先理解 source table 是什么

在 jrtplib 里，RTPSession 内部维护了一张 source table，可以理解为当前 RTP 会话已知的参与者列表。

每个 source 通常对应一个远端参与者，里面保存的信息包括：

- SSRC
- RTP 地址
- RTCP 地址
- 收发统计信息
- RTCP 状态

当 jrtplib 收到一个新的远端数据源时，会把它加入 source table；当这个源超时、退出或被清理时，又会把它从 source table 中删除。

OnNewSource、OnRemoveSource、OnBYEPacket 就是围绕这个生命周期触发的回调函数。

### 9.2 OnNewSource 的作用

基类定义在 [3rd/include/jrtplib3/rtpsession.h](../../../3rd/include/jrtplib3/rtpsession.h#L492)：

- 当一个新的 source 条目被加入 source table 时调用。

你项目里的重写在 [SipSupService/include/Gb28181Session.h](../../../SipSupService/include/Gb28181Session.h#L105)。

它的实际处理逻辑是：

1. 先打印日志。
2. 如果这个 source 是自己的 SSRC，则直接返回。
3. 优先从 RTP 地址中取出对端 IP 和端口。
4. 如果没有 RTP 地址，则尝试从 RTCP 地址推导 RTP 端口。
5. 构造 RTPIPv4Address。
6. 调用 AddDestination，把这个对端加入发送目标列表。

这说明 OnNewSource 在你项目中的业务含义是：

- 当发现一个新的远端参与者时，自动把它加入 RTP 发送目的地列表。

换句话说，它不是简单记录“新源来了”，而是在建立一条新的会话发送关系。

### 9.3 OnRemoveSource 的作用

基类定义在 [3rd/include/jrtplib3/rtpsession.h](../../../3rd/include/jrtplib3/rtpsession.h#L495)：

- 当一个 source 条目即将从 source table 中删除时调用。

你项目里的重写在 [SipSupService/include/Gb28181Session.h](../../../SipSupService/include/Gb28181Session.h#L140)。

它的实际处理逻辑是：

1. 如果是自己的 SSRC，直接返回。
2. 如果这个源已经收到过 BYE，则直接返回。
3. 取出对端地址和端口。
4. 构造 RTPIPv4Address。
5. 调用 DeleteDestination，把这个目标从发送列表中删除。

它在你项目中的业务含义是：

- 当某个远端参与者即将从 jrtplib 的源表里移除时，同步把它从发送目标中清理掉。

这里的关键点是这一句：

- 如果 `srcdat->ReceivedBYE()` 为真，就不在这里删。

原因是：

- 对端如果是显式发了 RTCP BYE 包退出，会走 OnBYEPacket 这条逻辑。
- OnRemoveSource 更偏向处理“超时移除”或“内部清理移除”。

这样可以避免重复删除同一个 destination。

### 9.4 OnBYEPacket 的作用

基类定义在 [3rd/include/jrtplib3/rtpsession.h](../../../3rd/include/jrtplib3/rtpsession.h#L536)：

- 当一个 source 的 RTCP BYE 包被处理后调用。

你项目里的重写在 [SipSupService/include/Gb28181Session.h](../../../SipSupService/include/Gb28181Session.h#L174)。

它的实际处理逻辑是：

1. 如果是自己的 SSRC，直接返回。
2. 解析出对端 IP 和端口。
3. 构造 RTPIPv4Address。
4. 调用 DeleteDestination，将该对端从发送列表中移除。

它的业务含义是：

- 对端明确通过 RTCP BYE 告诉你“我要离开这个会话了”，此时立即停止继续向它发送 RTP 或 RTCP 数据。

这属于“主动结束”的场景。

### 9.5 三者之间的区别

这三个回调虽然都和 source 生命周期有关，但层次不同：

#### OnNewSource

- 关注的是：发现新参与者。
- 触发点是：source 被加入 source table。
- 你这里的作用是：把新参与者加入发送目的地。

#### OnRemoveSource

- 关注的是：参与者即将被移除。
- 触发点是：source 被清理出 source table。
- 你这里的作用是：把已失效参与者从发送目的地中移除。

#### OnBYEPacket

- 关注的是：协议层收到了 BYE 控制消息。
- 触发点是：收到并处理了 RTCP BYE。
- 你这里的作用是：对端主动结束会话时，立即从发送目的地中删除。

所以可以把它们理解成下面这个关系：

1. OnNewSource：建立关系。
2. OnBYEPacket：对端主动告知退出，立即断开。
3. OnRemoveSource：对端已失效或被清理时做收尾。

### 9.6 在你这个项目里的整体作用

在 [SipSupService/include/Gb28181Session.h](../../../SipSupService/include/Gb28181Session.h) 这份实现里，这三个回调共同承担了一件事：

- 用 jrtplib 的 source table 生命周期，维护一份“当前有效的 RTP 发送目标列表”。

具体表现为：

1. 谁先成为一个合法的新源，就把谁加入 destination。
2. 谁显式发送 BYE，就立即移出 destination。
3. 谁虽然没发 BYE，但已经超时或被内部清理，也要移出 destination。

这使得会话的发送目标能够跟随远端参与者的在线状态动态更新。

### 9.7 一句话总结

OnNewSource、OnRemoveSource、OnBYEPacket 都是 RTPSession 提供的生命周期回调：

- OnNewSource 用于发现并接纳新的远端源。
- OnRemoveSource 用于清理即将失效的远端源。
- OnBYEPacket 用于处理对端显式发送的 BYE 退出行为。

在你当前项目中，它们被进一步用于自动维护 RTP 会话的 destination 列表。

