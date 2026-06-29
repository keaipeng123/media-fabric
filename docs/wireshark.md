# 抓包与排查

更新日期：2026-06-29

## 抓包目标

阶段 0 重点抓清楚这些链路：

- REGISTER 注册与 401/200 响应。
- MESSAGE keepalive 心跳。
- Catalog Query/Response。
- INVITE/200 OK/ACK。
- BYE。
- RTP/PS 媒体包。
- RTCP RR/SR/BYE。

## 默认端口

| 角色 | SIP 端口 | RTP 端口范围 |
| --- | --- | --- |
| SipSupService | `5061` | `20000-30000` |
| SipSubService | `7101` | `30000-40000` |

本地控制 TCP 端口：

- `SipSupService`：`11300`
- `SipSubService`：`11300`

注意：当前配置使用 `127.0.1`，Linux 环境下需要确认该地址是否符合本机网络配置。若抓本地回环流量，Wireshark 通常选择 `lo`。

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

RTP/RTCP：

```text
rtp || rtcp
```

按 RTP 端口范围粗过滤：

```text
udp portrange 20000-40000
```

本地控制端口：

```text
tcp.port == 11300
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
- 第二次 REGISTER 是否带 `Authorization`。

关联代码：

- [SipSubService/src/SipRegister.cpp](../SipSubService/src/SipRegister.cpp)
- [SipSupService/src/SipRegister.cpp](../SipSupService/src/SipRegister.cpp)

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

- [SipSubService/src/SipHeartBeat.cpp](../SipSubService/src/SipHeartBeat.cpp)
- [SipSupService/src/SipHeartBeat.cpp](../SipSupService/src/SipHeartBeat.cpp)

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
- 大目录分包时每个 NOTIFY 是否都有正确 `SN`。

关联代码：

- [SipSupService/src/GetCatalog.cpp](../SipSupService/src/GetCatalog.cpp)
- [SipSubService/src/SipDirectory.cpp](../SipSubService/src/SipDirectory.cpp)
- [SipSupService/src/SipDirectory.cpp](../SipSupService/src/SipDirectory.cpp)

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

关联代码：

- [SipSupService/src/OpenStream.cpp](../SipSupService/src/OpenStream.cpp)
- [SipSubService/src/SipGbPlay.cpp](../SipSubService/src/SipGbPlay.cpp)
- [SipSupService/src/SipGbPlay.cpp](../SipSupService/src/SipGbPlay.cpp)

## RTP/PS 检查点

检查项：

- RTP payload type 是否为 `96`。
- RTP timestamp 是否按同一帧保持一致、跨帧递增。
- sequence 是否连续。
- marker 是否在一帧最后一个包置位。
- 是否有 RTCP RR/SR。

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
