# 里程碑 4 完成审计清单

本文用于判断里程碑 4 是否真正完成。结论以当前代码、测试输出、Linux 目标环境构建结果和抓包证据为准；没有证据的项目不能视为完成。

## 当前结论

截至当前编辑环境检查，里程碑 4 已完成统一节点、能力模块边界、默认自测和接口级适配检查；尚未完成 Linux 目标环境的 PJSIP + JRTPLIB 真实链接、启动和 SIP/RTP 联合抓包验收。

当前阻塞项：

```text
3rd/lib/libjrtp.a 缺失
当前编辑环境不是 Linux，无法完成 PJSIP/JRTPLIB 的真实链接、启动和抓包验收
```

因此里程碑 4 目前不能标记为完成。

## 已有证据

本地默认构建与自测应保持通过：

```bash
scripts/verify-milestone4-linux.sh default
ctest --test-dir build --output-on-failure -R gb28181-server-self-test
```

通过标记：

```text
REGISTER_AUTH_RETRY=ok
INVITE_ACK=ok
routes=11
sent_messages=24
sessions=24
```

当前自测覆盖的关键能力：

```text
同一 gb28181-server 进程承载注册、被注册、保活、Catalog、RecordInfo、INVITE、ACK、BYE、媒体状态和业务查询能力
REGISTER 401 response -> Authorization REGISTER retry
REGISTER challenge、Digest 校验、nonce/nc replay 拒绝
Keepalive、Catalog、RecordInfo 请求与响应解析
INVITE 2xx 后 ACK 出站生成
ACK CSeq 校验、dialog 确认、BYE 释放媒体会话
RTP 包解析、payload 分片、marker 重组、PS/PES 基础解析、Annex-B NAL 识别
RTP adapter receive/send 抽象入口
stream.file 循环读取和媒体发送调度
```

当前非 Linux 编辑环境已完成接口级检查：

```bash
c++ -std=c++11 -Wall -Wextra -Wpedantic \
  -DPJ_M_X86_64=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 \
  -Isrc/core -Isrc/common/include -Isrc/media -Isrc/capabilities \
  -Isrc/adapters/pjsip -I3rd/include -I3rd/include/pjsip \
  -DGB28181_ENABLE_PJSIP \
  -c src/adapters/pjsip/PjsipStackAdapter.cpp -o /private/tmp/PjsipStackAdapter.o

c++ -std=c++11 -Wall -Wextra -Wpedantic \
  -DPJ_M_X86_64=1 -DPJ_IS_LITTLE_ENDIAN=1 -DPJ_IS_BIG_ENDIAN=0 \
  -Isrc/core -Isrc/common/include -Isrc/media -Isrc/capabilities \
  -Isrc/adapters/pjsip -I3rd/include -I3rd/include/pjsip \
  -DGB28181_ENABLE_PJSIP \
  -c src/core/SipStack.cpp -o /private/tmp/SipStack.o

c++ -std=c++11 -Wall -Wextra -Wpedantic \
  -Isrc/media -Isrc/common/include -I3rd/include \
  -c src/adapters/jrtplib/JrtplibRtpSessionAdapter.cpp \
  -o /private/tmp/JrtplibRtpSessionAdapter.o
```

这些检查只能证明接口语法边界，不等同于 Linux 链接和运行完成。

## 完成门禁

里程碑 4 完成前，必须在 Linux 目标环境依次通过以下命令。

1. 补齐第三方库后执行严格预检查：

```bash
scripts/prepare-jrtplib-linux.sh /path/to/jrtplib-3.11.2
```

```bash
GB28181_PREFLIGHT_STRICT=1 scripts/verify-milestone4-linux.sh preflight
```

预期结果：

```text
preflight: ready
```

2. 默认自测通过：

```bash
scripts/verify-milestone4-linux.sh default
```

3. PJSIP 构建与启动通过：

```bash
scripts/verify-milestone4-linux.sh pjsip-build
scripts/verify-milestone4-linux.sh pjsip-smoke
```

4. JRTPLIB 构建与启动通过：

```bash
scripts/verify-milestone4-linux.sh jrtplib-build
scripts/verify-milestone4-linux.sh jrtplib-smoke
```

5. 最终真实网络组合通过：

```bash
scripts/verify-milestone4-linux.sh full-build
scripts/verify-milestone4-linux.sh full-smoke
scripts/verify-milestone4-linux.sh full-capture
```

Linux 环境补齐第三方库后，可用一条命令顺序执行上述硬门禁：

```bash
scripts/verify-milestone4-linux.sh completion-gate
```

该命令会在 `full-capture` 后自动执行 `capture-audit`，复查报告状态、pcap 存在性、可读包数，并在安装 `tshark` 时输出 SIP/RTP display filter 计数。该命令通过仍只能证明构建、启动、抓包产物和基础计数链路通过；pcap 协议语义仍需按下方清单检查。

也可以对已有抓包结果单独复查：

```bash
scripts/verify-milestone4-linux.sh capture-audit artifacts/milestone4
scripts/verify-milestone4-linux.sh capture-audit artifacts/milestone4/milestone4-full-report-<timestamp>.txt
```

当前统一配置只从 `[node]` 启动一个本地 SIP listener；`peer.upstream.*.remote_port` 是远端平台端口，不会创建第二个本地 TCP listener。PJSIP 2.7.2 在旧迁移配置或重复 transport 场景下仍可能返回 `PJSIP_ETYPEEXISTS`，adapter 会把重复 TCP listener 记录为 warning 后继续启动；默认验收应以单 listener 模型为准。

## 抓包证据

`full-capture` 应输出：

```text
artifacts/milestone4/milestone4-full-sip-<timestamp>.pcap
artifacts/milestone4/milestone4-full-rtp-<timestamp>.pcap
artifacts/milestone4/milestone4-full-report-<timestamp>.txt
```

报告文件记录主机、配置、抓包网卡、命令状态、pcap 路径、pcap 字节数和 pcap 包数。`full-capture` 和 `capture-audit` 会把 tcpdump 异常、pcap 缺失/为空或 pcap 包数为 0 视为失败；报告是证据索引，不替代 pcap 内容检查。

SIP pcap 必须能证明：

```text
REGISTER 401 challenge
REGISTER Authorization retry
REGISTER 200 OK
REGISTER replay 或 nc replay 被拒绝
Keepalive MESSAGE 与响应
Catalog MESSAGE 与响应
RecordInfo MESSAGE 与响应
INVITE 200 OK
ACK 携带正确 Call-ID、CSeq、From tag、To tag，并发往 Contact target
BYE 正常关闭
错误 dialog BYE 返回 481
```

RTP pcap 必须能证明：

```text
RTP payload type 96
sequence 连续递增
timestamp 合理递增
marker 与帧边界匹配
payload 可承载 PS 数据
程序日志显示 RTP/PS/Annex-B 链路被实际触发
```

## 完成后回填

Linux 验收通过后，需要回填：

```text
docs/milestone4-node-capabilities.md 的“未完成项”
docs/wireshark.md 的抓包结论
本文件的“当前结论”
```

回填时应保留实际命令、退出码、关键日志和 pcap 文件路径。
