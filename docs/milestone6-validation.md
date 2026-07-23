# 里程碑 6：现有链路验证

目标：验证统一后的 `media-fabric` 仍能完整覆盖既有 GB/T 28181 链路。

## 验证范围

```text
下级注册与媒体发送
  -> REGISTER
  -> Keepalive MESSAGE
  -> Catalog Response
  -> INVITE / 200 OK / ACK
  -> H.264 -> PS Mux -> RTP Send

上级注册服务、目录查询、开流与媒体接收
  -> REGISTER Auth / Response
  -> Keepalive Handle
  -> Catalog Query
  -> INVITE
  -> RTP Receive -> PS Demux -> H.264 Frame
```

## 验收标准

- 上下级能够完成注册和心跳保活。
- 上级能够查询下级目录，并按目录设备 ID 发起实时预览。
- 下级能够响应 INVITE；上级完成 INVITE/200 OK/ACK 后收到 RTP/PS。
- `mfcli streams` 中的接收包、帧和字节统计持续递增。
- 执行 `mfcli bye <device-id>` 后双方会话关闭，RTP 端口归还。

真实 SIP/RTP 抓包时，使用 [抓包与排查](wireshark.md) 中的过滤器与检查项。
