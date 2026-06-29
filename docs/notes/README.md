# Notes 维护索引

原 `notes` 目录中的长期知识笔记已迁移到本目录，后续统一在 `docs/notes/` 中维护。

## 目录说明

- `pjsip/`：PJSIP 初始化、GB28181 注册、心跳、目录、INVITE 等信令笔记。
- `jrtplib/`：RTP/RTCP、PS、H.264/H.265、PTS/DTS 等媒体链路笔记。
- `C++和C语法整理/`：C++ 基础语法、线程、定时器、RAII、IO 复用等工程基础笔记。
- `assets/`：文档引用的截图和流程图资源。
- `第三方库安装.md`：当前项目依赖库的整理说明。
- `markdown语法库.md`：Markdown 写作语法备忘。

## 推荐阅读顺序

1. [第三方库安装.md](第三方库安装.md)
2. [pjsip/pjsip初始化_SipCore.md](pjsip/pjsip初始化_SipCore.md)
3. [pjsip/GB28181注册_REGISTER_SipRegister.md](pjsip/GB28181注册_REGISTER_SipRegister.md)
4. [pjsip/GB28181心跳_MESSAGE_SipHeartBeat.md](pjsip/GB28181心跳_MESSAGE_SipHeartBeat.md)
5. [pjsip/GB28181目录.md](pjsip/GB28181目录.md)
6. [pjsip/invite.md](pjsip/invite.md)
7. [jrtplib/RTPSession.md](jrtplib/RTPSession.md)
8. [jrtplib/H264和PS流的区别.md](jrtplib/H264和PS流的区别.md)
9. [jrtplib/H264中NALU_IDR_I帧_P帧_B帧_SPS_PPS整理.md](jrtplib/H264中NALU_IDR_I帧_P帧_B帧_SPS_PPS整理.md)

## 代码入口索引

注册：

- 下级发起 REGISTER：[../../SipSubService/src/SipRegister.cpp](../../SipSubService/src/SipRegister.cpp)
- 上级处理 REGISTER：[../../SipSupService/src/SipRegister.cpp](../../SipSupService/src/SipRegister.cpp)

心跳：

- 下级发送 keepalive：[../../SipSubService/src/SipHeartBeat.cpp](../../SipSubService/src/SipHeartBeat.cpp)
- 上级处理 keepalive：[../../SipSupService/src/SipHeartBeat.cpp](../../SipSupService/src/SipHeartBeat.cpp)

目录：

- 上级发起 Catalog：[../../SipSupService/src/GetCatalog.cpp](../../SipSupService/src/GetCatalog.cpp)
- 下级响应 Catalog：[../../SipSubService/src/SipDirectory.cpp](../../SipSubService/src/SipDirectory.cpp)
- 上级解析 Catalog：[../../SipSupService/src/SipDirectory.cpp](../../SipSupService/src/SipDirectory.cpp)

开流：

- 上级发起 INVITE：[../../SipSupService/src/OpenStream.cpp](../../SipSupService/src/OpenStream.cpp)
- 下级处理 INVITE/BYE：[../../SipSubService/src/SipGbPlay.cpp](../../SipSubService/src/SipGbPlay.cpp)
- 上级处理 SDP 与 RTP 会话：[../../SipSupService/src/SipGbPlay.cpp](../../SipSupService/src/SipGbPlay.cpp)

媒体：

- 下级 PS mux/RTP send：[../../SipSubService/src/Gb28181Session.cpp](../../SipSubService/src/Gb28181Session.cpp)
- 上级 RTP receive/PS demux：[../../SipSupService/src/Gb28181Session.cpp](../../SipSupService/src/Gb28181Session.cpp)

## 维护约定

- 长期有价值的协议、源码、排障笔记放在这里。
- 每日流水账不再保留为维护文档。
- 新增文档尽量按主题归档，不再使用 day 日志组织。
- 文档中引用项目源码时，优先使用相对仓库根目录可读的链接。
