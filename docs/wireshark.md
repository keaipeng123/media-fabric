# 抓包与排查

更新日期：2026-07-02

## 抓包目标

里程碑 4 在 Linux 目标环境重点抓清楚这些链路：

- REGISTER 注册、401 Digest challenge、带 Authorization 的重试和 200 响应。
- MESSAGE keepalive 心跳。
- Catalog Query/Response。
- INVITE/200 OK/ACK。
- BYE。
- RTP/PS 媒体包。
- RTCP RR/SR/BYE。

## 默认端口

| 角色 | SIP 端口 | RTP 端口范围 |
| --- | --- | --- |
| 统一入口 sup endpoint | `5061` | `20000-30000` |
| 统一入口 sub endpoint | `7101` | `30000-40000` |

注意：当前示例配置使用 `127.0.1`，Linux 环境下需要确认该地址是否符合本机网络配置。若抓本地回环流量，Wireshark 通常选择 `lo`。

## Wireshark 过滤器

SIP：

```text
sip
```

按端口过滤 SIP：

```text
udp.port == 5061 || tcp.port == 5061 || udp.port == 7101 || tcp.port == 7101
```

GB28181 常见消息：

```text
sip.Method == "REGISTER"
sip.Method == "MESSAGE"
sip.Method == "INVITE"
sip.Method == "BYE"
```

跟踪单个 dialog：

```text
sip.Call-ID contains "<call-id>"
```

RTP/RTCP：

```text
rtp || rtcp
```

按 RTP 端口范围粗过滤：

```text
udp portrange 20000-40000
```

## REGISTER 检查点

期望流程：

```text
Sub -> Sup REGISTER
Sup -> Sub 401 Unauthorized
Sub -> Sup REGISTER with Authorization
Sup -> Sub 200 OK
```

检查项：

- `From` / `To` / `Contact` 中的 SIP ID 是否符合配置。
- `Expires` 是否符合下级配置。
- 如果开启鉴权，`WWW-Authenticate` 中 realm 是否为 `1000000000`。
- 第二次 REGISTER 是否带 `Authorization`，其中 `username`、`realm`、`nonce`、`uri`、`response` 与 401 challenge 匹配。
- replay 请求是否被拒绝。

关联代码：

- [src/capabilities/GB28181Capabilities.cpp](../src/capabilities/GB28181Capabilities.cpp)
- 历史对照：[SipSubService/src/SipRegister.cpp](../SipSubService/src/SipRegister.cpp)、[SipSupService/src/SipRegister.cpp](../SipSupService/src/SipRegister.cpp)

## Keepalive 检查点

期望 XML：

```xml
<Notify>
  <CmdType>keepalive</CmdType>
  <SN>...</SN>
  <DeviceID>...</DeviceID>
  <Status>OK</Status>
</Notify>
```

检查项：

- `MESSAGE` body 是否为 `Application/MANSCDP+xml`。
- `DeviceID` 是否为下级 SIP ID。
- 上级是否回复 `200 OK`。

关联代码：

- [src/capabilities/GB28181Capabilities.cpp](../src/capabilities/GB28181Capabilities.cpp)
- 历史对照：[SipSubService/src/SipHeartBeat.cpp](../SipSubService/src/SipHeartBeat.cpp)、[SipSupService/src/SipHeartBeat.cpp](../SipSupService/src/SipHeartBeat.cpp)

## Catalog 检查点

期望流程：

```text
Sup -> Sub MESSAGE Query Catalog
Sub -> Sup 200 OK
Sub -> Sup NOTIFY/Response Catalog
```

检查项：

- Query 中 `CmdType` 是否为 `Catalog`。
- Response 中 `DeviceList`、`SumNum`、`DeviceID` 是否符合 `catalog.json`。
- Response Item 常用字段是否包含 `Name`、`Manufacturer`、`Model`、`Owner`、`CivilCode`、`Parental`、`ParentID`、`SafetyWay`、`RegisterWay`、`Secrecy`、`Status`。
- 大目录分包时每个 MESSAGE 是否都有正确 `SN`。

关联代码：

- [src/capabilities/GB28181Capabilities.cpp](../src/capabilities/GB28181Capabilities.cpp)
- 历史对照：[SipSupService/src/GetCatalog.cpp](../SipSupService/src/GetCatalog.cpp)、[SipSubService/src/SipDirectory.cpp](../SipSubService/src/SipDirectory.cpp)、[SipSupService/src/SipDirectory.cpp](../SipSupService/src/SipDirectory.cpp)

## INVITE 检查点

期望 SDP：

```text
m=video <port> RTP/AVP 96
a=rtpmap:96 PS/90000
a=recvonly
```

下级 200 OK 期望：

```text
m=video <port> RTP/AVP 96
a=rtpmap:96 PS/90000
a=sendonly
```

检查项：

- `Subject` 是否包含设备 ID 和平台 ID。
- `c=` 行 IP 是否为收流地址。
- `m=` 行端口是否与 RTP 实际端口一致。
- TCP 模式下 `a=setup` 主被动方向是否互补。
- 200 OK 的 `Call-ID` 与 INVITE 一致，`CSeq` number 一致且 method 为 `INVITE`。
- ACK 的 request target 优先指向 200 OK 的 `Contact`，`Call-ID` 与 INVITE/200 OK 一致，`CSeq` number 一致且 method 为 `ACK`。
- ACK 的 `From` tag 继承 200 OK 的 `From` tag，`To` tag 继承 200 OK 的 `To` tag。
- BYE 使用同一 dialog，错误 dialog 应返回 `481`，合法 BYE 应返回 `200`。

关联代码：

- [src/capabilities/GB28181Capabilities.cpp](../src/capabilities/GB28181Capabilities.cpp)
- [src/adapters/pjsip/PjsipStackAdapter.cpp](../src/adapters/pjsip/PjsipStackAdapter.cpp)
- 历史对照：[SipSupService/src/OpenStream.cpp](../SipSupService/src/OpenStream.cpp)、[SipSubService/src/SipGbPlay.cpp](../SipSubService/src/SipGbPlay.cpp)、[SipSupService/src/SipGbPlay.cpp](../SipSupService/src/SipGbPlay.cpp)

## RTP/PS 检查点

检查项：

- RTP payload type 是否为 `96`。
- RTP timestamp 是否按同一帧保持一致、跨帧递增。
- sequence 是否连续。
- marker 是否在一帧最后一个包置位。
- SSRC 是否与 SDP `y=` 或配置侧期望一致。
- PS payload 是否可解析出完整 pack/PES，视频 PES 中是否为 Annex-B H.264/H.265。
- 是否有 RTCP RR/SR。

## Linux 验收顺序

1. 先做目标环境 preflight，再跑默认构建和内部自测：

```bash
scripts/verify-milestone4-linux.sh preflight
scripts/verify-milestone4-linux.sh default
ctest --test-dir build --output-on-failure -R gb28181-server-self-test
```

2. PJSIP adapter 构建、启动并抓 SIP：

```bash
scripts/verify-milestone4-linux.sh pjsip-build
scripts/verify-milestone4-linux.sh pjsip-smoke
```

3. JRTPLIB adapter 补齐 `3rd/lib/libjrtp.a` 后构建、启动并抓 RTP：

```bash
scripts/verify-milestone4-linux.sh jrtplib-build
scripts/verify-milestone4-linux.sh jrtplib-smoke
```

4. 同时启用 PJSIP + JRTPLIB，验证最终真实网络组合，并抓 SIP/RTP：

```bash
scripts/verify-milestone4-linux.sh full-build
scripts/verify-milestone4-linux.sh full-smoke
scripts/verify-milestone4-linux.sh full-capture
```

也可以直接运行完整门禁：

```bash
scripts/verify-milestone4-linux.sh completion-gate
```

对已有抓包产物做基础复查：

```bash
scripts/verify-milestone4-linux.sh capture-audit artifacts/milestone4
```

`full-capture` 默认在 `lo` 上抓 10 秒，并输出：

```text
artifacts/milestone4/milestone4-full-sip-<timestamp>.pcap
artifacts/milestone4/milestone4-full-rtp-<timestamp>.pcap
artifacts/milestone4/milestone4-full-report-<timestamp>.txt
```

tcpdump 异常、pcap 缺失/为空或 pcap 包数为 0 时，`full-capture` 和 `capture-audit` 会失败。报告文件只记录抓包产物和待检查项；SIP/RTP 内容仍要按本页检查。

可用环境变量调整：

```bash
GB28181_CAPTURE_IFACE=lo
GB28181_CAPTURE_SECONDS=10
GB28181_CAPTURE_DIR=artifacts/milestone4
```

5. 抓包结论回填到 [milestone4-node-capabilities.md](milestone4-node-capabilities.md) 的“未完成项”和 [milestone4-completion-audit.md](milestone4-completion-audit.md)。

Wireshark 可能无法自动把 payload type 96 识别成 PS。可以先用 RTP 基础字段验证，再结合程序日志确认 PS demux 是否成功。

关联代码：

- [SipSubService/src/Gb28181Session.cpp](../SipSubService/src/Gb28181Session.cpp)
- [SipSupService/src/Gb28181Session.cpp](../SipSupService/src/Gb28181Session.cpp)

## 保存抓包建议

建议后续保存这些典型样本：

```text
docs/captures/register-auth.pcapng
docs/captures/keepalive.pcapng
docs/captures/catalog.pcapng
docs/captures/invite-rtp-udp.pcapng
docs/captures/bye.pcapng
```

当前仓库尚未加入抓包样本。阶段 0 验证链路跑通后，再补充对应文件和截图。
