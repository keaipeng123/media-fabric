# H264 中 NALU、IDR、I/P/B 帧、SPS、PPS 整理

本文结合当前工程中的代码，对 H264 码流里的几个核心概念做一份统一整理，重点说明：

1. NALU 是什么
2. IDR、I 帧、P 帧、B 帧是什么关系
3. SPS、PPS 的作用是什么
4. 起始码到底能不能区分 I/P/B
5. 当前工程里的 `sBuf`、`SendPacket`、`GetH264pic` 分别在做什么

结合的代码位置：

1. [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L53)
2. [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L87)
3. [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L177)
4. [SipSupService/include/SipDef.h](SipSupService/include/SipDef.h#L56)

## 1. 先看当前代码在做什么

在当前工程里，PS 解封装后的裸音视频数据会进入回调 `ps_demux_callback`。

其中视频裸流会先暂存在 `pProc->sBuf` 中，相关逻辑在 [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L70)。

这里的 `sBuf` 可以理解为：

1. 一帧 H264 裸流的暂存缓冲区。
2. 因为一次回调给到的 `data` 不一定是完整一帧，所以代码要先把同一帧的数据拼起来。
3. 当检测到已经切换到下一帧，或者切换到另一种流类型时，才把上一帧累计好的 `sBuf` 送给 `SendPacket` 处理。

也就是说：

```text
PS 解封装后的零散 H264 数据 -> 累积到 sBuf -> 凑成一帧/一组完整裸流 -> SendPacket 进一步解析
```

然后在 [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L177) 的 `SendPacket` 中，代码会：

1. 判断当前是不是 H264 视频流。
2. 识别裸流开头是不是 H264 NALU 起始码。
3. 取出第一个 NALU 的类型 `nalType`。
4. 如果当前第一个 NALU 是 SPS，就调用 `GetH264pic` 从 SPS 中解析宽高和帧率。
5. 最后把这些信息填入 `StreamHeader`，供后面使用。

## 2. NALU 是什么

NALU 是 H264 的基本传输单元，全称是 Network Abstraction Layer Unit。

简单理解：

1. 一段 H264 码流不是一整块不可分的数据。
2. 它是由很多个 NALU 串起来组成的。
3. 每个 NALU 前面通常有起始码，用来标记边界。

在 Annex B 格式的裸 H264 中，常见起始码有两种：

```text
00 00 01
00 00 00 01
```

注意，这两个起始码只是“分隔符”，表示下一个 NALU 从这里开始。

它们本身不表示这是 I 帧、P 帧、B 帧、SPS、PPS 还是 IDR。

真正的类型要看起始码后面的 NALU 头。

## 3. 当前代码如何取 NALU 类型

在 [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L198) 中，代码先判断：

1. 如果前面是 `00 00 00 01`，那 NALU 头在第 5 个字节。
2. 如果前面是 `00 00 01`，那 NALU 头在第 4 个字节。

然后通过下面这个操作取类型：

```cpp
nalType = data[x] & 0x1F;
```

这里取的是 NALU 头低 5 位，也就是 `nal_unit_type`。

H264 中常见的 `nal_unit_type` 包括：

1. `1`：非 IDR slice
2. `5`：IDR slice
3. `7`：SPS
4. `8`：PPS

所以如果起始码后面的字节是：

1. `0x67`，通常代表 SPS
2. `0x68`，通常代表 PPS
3. `0x65`，通常代表 IDR slice
4. `0x41`、`0x61` 等，通常代表非 IDR slice

## 4. SPS 和 PPS 是什么

SPS 是 Sequence Parameter Set，序列参数集。

PPS 是 Picture Parameter Set，图像参数集。

它们不是视频画面本身，而是解码所需的参数说明。

### 4.1 SPS 的作用

SPS 里一般会包含：

1. 编码 profile、level
2. 宽高信息
3. 帧编号相关参数
4. 一些时序和参考帧相关参数

因此，分辨率通常可以从 SPS 中解析出来。

当前代码里的 `GetH264pic` 就是在做这件事，位置在 [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L15)。

它先调用 `mpeg4_annexbtomp4` 把 Annex B 格式的 H264 解析出 AVC 结构，再取出 SPS，然后通过 `h264_configure_parse` 解析：

1. `info->width`
2. `info->height`
3. `info->max_framerate`

最后这些信息会在 `SendPacket` 中写进 `StreamHeader`。

### 4.2 PPS 的作用

PPS 一般包含：

1. slice 相关控制参数
2. 熵编码方式参数
3. 参考图像管理相关参数

PPS 通常要配合 SPS 一起使用，解码器只有拿到 SPS 和 PPS，才能正确解后面的图像 slice。

## 5. IDR 帧是什么

IDR 是 Instantaneous Decoding Refresh。

它是一种特殊的关键图像。它的特点是：

1. 解码器可以从这里重新建立参考关系。
2. IDR 之后的图像不会依赖 IDR 之前的参考帧。
3. 对播放器来说，它是一个很好的随机接入点。

在 NALU 层面，IDR 通常表现为：

1. 起始码
2. NALU 头类型为 `5`

也就是常见形式：

```text
00 00 00 01 65 ...
```

这里的 `65` 中低 5 位是 `5`，表示 IDR slice。

## 6. I 帧、P 帧、B 帧分别是什么

I/P/B 是图像编码方式的分类，不是起始码分类，也不完全等同于 NALU 类型分类。

### 6.1 I 帧

I 帧是帧内编码图像。

特点是：

1. 主要依赖本帧内部信息恢复图像。
2. 不需要参考前面的图像内容才能解码。
3. 通常码率较大，但适合作为切入点。

### 6.2 P 帧

P 帧是前向预测图像。

特点是：

1. 会参考前面的图像。
2. 压缩率比 I 帧高。
3. 一旦前面的参考帧出问题，后面的 P 帧也可能受影响。

### 6.3 B 帧

B 帧是双向预测图像。

特点是：

1. 同时参考前后图像。
2. 压缩效率更高。
3. 解码和时序处理更复杂。

## 7. IDR 和 I 帧是什么关系

这是最容易混淆的点。

关系是：

1. IDR 通常属于 I 帧中的一种特殊情况。
2. IDR 是一种更强的“刷新点”。
3. 不是所有 I 帧都是 IDR。

可以这么理解：

1. I 帧强调的是“编码方式”。
2. IDR 强调的是“是否能作为新的解码刷新点”。

所以：

1. IDR 往往是关键帧。
2. 普通 I 帧不一定具有 IDR 那种彻底断开历史参考关系的能力。

## 8. I/P/B 帧的 NALU 起始码是什么

这个问题最容易答错。

正确答案是：I 帧、P 帧、B 帧没有各自专属的起始码。

它们前面的起始码仍然只是：

```text
00 00 01
00 00 00 01
```

也就是说：

1. I 帧前面可以是 `00 00 01`
2. P 帧前面也可以是 `00 00 01`
3. B 帧前面也可以是 `00 00 01`
4. 它们同样也都可能是 `00 00 00 01`

因此，起始码只能告诉你：

1. 这里开始了一个新的 NALU

不能告诉你：

1. 这是 I 帧
2. 这是 P 帧
3. 这是 B 帧

## 9. 那么 I/P/B 到底怎么区分

要分两步看。

### 9.1 第一步：看 `nal_unit_type`

1. 如果 `nal_unit_type = 5`，说明这是 IDR slice。
2. 如果 `nal_unit_type = 1`，说明这是非 IDR slice。

但这里还不能完全区分 I/P/B。

因为普通 I 帧、P 帧、B 帧很多时候都可能被装在 `nal_unit_type = 1` 的 slice 里。

### 9.2 第二步：继续解析 slice header

必须进一步解析 slice header 里的 `slice_type` 才能知道它究竟是：

1. I slice
2. P slice
3. B slice

所以结论是：

1. 起始码不能区分 I/P/B。
2. 只看 `nal_unit_type` 也不能完整区分 I/P/B。
3. 要准确判断 I/P/B，必须继续读 slice header。

## 10. 当前 `SendPacket` 的判断准确到什么程度

当前 [SipSupService/src/Gb28181Session.cpp](SipSupService/src/Gb28181Session.cpp#L214) 里的逻辑是：

1. 如果 `nalType == 7`，就把它视为关键帧相关数据。
2. 然后调用 `GetH264pic` 从 SPS 中解析宽高和帧率。

这套逻辑在工程上可以工作，但要注意它的语义边界：

1. `nalType == 7` 只说明当前第一个 NALU 是 SPS。
2. 它不等价于“当前就是 IDR slice”。
3. 它更像是在说“当前这包数据包含关键帧前常见的参数集信息”。

很多码流在关键帧附近的组织方式确实是：

```text
00 00 00 01 67  -> SPS
00 00 00 01 68  -> PPS
00 00 00 01 65  -> IDR slice
```

所以工程里常常把“遇到 SPS”当成“关键帧开始附近”的信号，这在很多场景下成立，但它不是严格意义上的 IDR 判定。

更严格的判定应该是：

1. 扫描整包中的多个 NALU
2. 看后续是否存在 `nal_unit_type = 5`
3. 如果存在，才能确定里面含有 IDR slice

## 11. 当前工程里的参数最终写到哪里

在 [SipSupService/include/SipDef.h](SipSupService/include/SipDef.h#L56) 中，`StreamHeader` 定义了：

1. `type`：媒体类型
2. `length`：负载长度
3. `videoH`：视频高
4. `videoW`：视频宽
5. `format[4]`：其他参数

当前 `SendPacket` 中，视频参数主要写到：

1. `header->videoH`
2. `header->videoW`
3. `header->format[0]`，这里被用来存帧率
4. `header->format[1]`，这里被用来存当前代码定义下的关键帧标志
5. `header->format[2]`，这里被用来存编码类型

## 12. 一组典型 H264 关键帧附近的数据示意

一个常见的关键帧附近裸流可能长这样：

```text
00 00 00 01 67 ...   SPS
00 00 00 01 68 ...   PPS
00 00 00 01 65 ...   IDR slice
```

而普通预测帧可能更像：

```text
00 00 00 01 41 ...   非 IDR slice
```

但注意：

1. 这里的 `00 00 00 01` 只是 NALU 起始码。
2. 真正区分类型的是后面的 NALU 头以及 slice header。

## 13. 一句话总结

一句话概括：

1. NALU 是 H264 的基本单元。
2. SPS/PPS 是参数集，不是视频画面本身。
3. IDR 是特殊的关键图像，通常对应 `nal_unit_type = 5`。
4. I/P/B 是编码图像类型，不能只靠起始码区分。
5. 起始码只有“分隔 NALU”的作用，常见就是 `00 00 01` 和 `00 00 00 01`。
6. 当前工程里的 `sBuf` 是拼帧缓冲区，`GetH264pic` 负责从 SPS 解析分辨率和帧率，`SendPacket` 负责识别首个 NALU 类型并写入头部信息。