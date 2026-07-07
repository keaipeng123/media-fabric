# 第三方库补齐说明

本文记录里程碑 4 在 Linux 目标环境继续验收时需要关注的第三方静态库。

## 当前状态

仓库当前已经包含：

```text
3rd/include/jrtplib3/
3rd/include/jthread/
3rd/lib/libjthread.a
```

仓库当前缺少：

```text
3rd/lib/libjrtp.a
```

因此：

```bash
scripts/verify-milestone4-linux.sh preflight
```

会报告 `JRTPLIB libjrtp` 缺失；设置 `GB28181_PREFLIGHT_STRICT=1` 时会失败。这是预期的硬门禁。

## 推荐版本

历史安装笔记中记录的版本为：

```text
jrtplib-3.11.2
jthread-1.3.3
```

JThread 当前已经有 `3rd/lib/libjthread.a`，通常只需要补齐 JRTPLIB 的 `libjrtp.a`。

## Linux 构建 JRTPLIB

在 Linux 目标环境准备 `jrtplib-3.11.2` 源码后，推荐直接执行：

```bash
scripts/prepare-jrtplib-linux.sh /path/to/jrtplib-3.11.2
```

脚本会执行 CMake 构建、安装，并把 `libjrtp.a` 或 `libjrtplib.a` 复制为：

```text
3rd/lib/libjrtp.a
```

也可以手动执行：

```bash
cmake -S /path/to/jrtplib-3.11.2 -B /tmp/jrtplib-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_INSTALL_PREFIX=/tmp/jrtplib-install

cmake --build /tmp/jrtplib-build
cmake --install /tmp/jrtplib-build
```

然后把静态库复制到项目目录：

```bash
cp /tmp/jrtplib-install/lib/libjrtp.a 3rd/lib/libjrtp.a
```

如果安装路径下的库名是 `libjrtplib.a`，需要确认它确实是 JRTPLIB 静态库；当前项目 preflight 要求 `libjrtp.a`，建议统一复制/命名为：

```text
3rd/lib/libjrtp.a
```

## 验证

补齐后在 Linux 目标环境运行：

```bash
GB28181_PREFLIGHT_STRICT=1 scripts/verify-milestone4-linux.sh preflight
scripts/verify-milestone4-linux.sh jrtplib-build
scripts/verify-milestone4-linux.sh full-build
```

如果构建通过，再继续启动和抓包：

```bash
scripts/verify-milestone4-linux.sh jrtplib-smoke
scripts/verify-milestone4-linux.sh full-smoke
```

最终按 [wireshark.md](wireshark.md) 完成 SIP/RTP 抓包验收。
