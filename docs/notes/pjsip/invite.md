# SDP 构造时为什么有的成员要 `pj_pool_zalloc`，有的不需要

## 问题

在构造 SDP 时，像下面这类代码需要先调用 `pj_pool_zalloc`：

```cpp
pjmedia_sdp_media* m = (pjmedia_sdp_media*)pj_pool_zalloc(
	GBOJ(gSipServer)->GetPool(),
	sizeof(pjmedia_sdp_media)
);
sdp->media[0] = m;
```

但下面这类代码却不需要再额外创建内存：

```cpp
m->desc.fmt_count = 1;
m->desc.fmt[0] = pj_str("96");
```

疑问在于：`desc.fmt[0]` 的类型也是 `pj_str_t`，为什么它不需要像 `m` 一样单独分配？

---

## 核心结论

区别不在于“它是不是 `pj_str_t`”，而在于：

1. 这个成员是不是对象内部已经自带的内存。
2. 这个成员是不是只是一个指针槽位，需要另外指向一个独立对象。

简单说：

- 非指针成员、内嵌结构体、定长数组：跟随父对象一起分配。
- 指针成员：父对象里只有一个地址位，真正的对象要单独分配。

---

## 为什么 `m` 需要 `pj_pool_zalloc`

`sdp->media` 的定义是：

```cpp
pjmedia_sdp_media *media[PJMEDIA_MAX_SDP_MEDIA];
```

这里的 `media[0]` 本质上只是一个指针变量，它只能存地址，本身并不包含 `pjmedia_sdp_media` 实体。

所以这句：

```cpp
sdp->media[0] = m;
```

前提是 `m` 必须已经指向一块真实存在的 `pjmedia_sdp_media` 内存，因此要先分配：

```cpp
pjmedia_sdp_media* m = (pjmedia_sdp_media*)pj_pool_zalloc(...);
```

同理，下面这些也都需要单独分配，因为它们都是指针：

```cpp
sdp->conn
m->attr[0]
m->attr[1]
```

---

## 为什么 `desc.fmt[0]` 不需要 `pj_pool_zalloc`

`pjmedia_sdp_media` 里的定义是：

```cpp
struct pjmedia_sdp_media
{
	struct
	{
		pj_str_t    media;
		pj_uint16_t port;
		unsigned    port_count;
		pj_str_t    transport;
		unsigned    fmt_count;
		pj_str_t    fmt[PJMEDIA_MAX_SDP_FMT];
	} desc;
	...
};
```

注意这里的 `fmt` 不是指针，而是一个定长数组：

```cpp
pj_str_t fmt[PJMEDIA_MAX_SDP_FMT];
```

这表示：当 `m` 这整个 `pjmedia_sdp_media` 对象被分配出来时，`desc` 和 `desc.fmt[0..31]` 的内存已经全部包含在 `m` 里面了。

也就是说，这句：

```cpp
pjmedia_sdp_media* m = (pjmedia_sdp_media*)pj_pool_zalloc(..., sizeof(pjmedia_sdp_media));
```

实际上已经一次性分配好了这些内容：

1. `m->desc`
2. `m->desc.media`
3. `m->desc.transport`
4. `m->desc.fmt_count`
5. `m->desc.fmt[0]` 到 `m->desc.fmt[31]`

所以后面：

```cpp
m->desc.fmt_count = 1;
m->desc.fmt[0] = pj_str("96");
```

并不是“创建 `fmt[0]`”，而只是对已经存在的那块内存做赋值。

---

## `pj_str_t` 到底是什么

`pj_str_t` 的定义非常轻量，本质上只是一个“字符串视图”结构：

```cpp
struct pj_str_t
{
	char *ptr;
	pj_ssize_t slen;
};
```

它本身只保存两件事：

1. `ptr`：指向字符数据的地址。
2. `slen`：字符串长度。

所以 `pj_str_t` 不等于字符串内容本身，它只是一个描述器。

---

## 为什么 `m->desc.fmt[0] = pj_str("96")` 不需要分配

这句代码做的事情只是：

```cpp
m->desc.fmt[0].ptr = "96";
m->desc.fmt[0].slen = 2;
```

它没有去创建新的字符串内存，只是让 `fmt[0]` 指向已有的字符串常量 `"96"`。

因为字符串常量的生命周期是整个进程，所以这里是安全的，也就不需要再 `pj_pool_zalloc`。

---

## 什么时候 `pj_str_t` 背后的内容需要额外分配

如果 `pj_str_t` 指向的数据不是静态常量，而是临时数据，就要特别小心生命周期问题。

例如下面这种写法：

```cpp
sdp->name = pj_str((char*)info.streamName.c_str());
```

这里 `pj_str_t` 指向的是 `std::string` 内部缓冲区，并没有发生拷贝。

如果后续这个 `std::string` 失效了，而 SIP/PJSIP 还在继续使用这段 SDP，那么 `ptr` 就会变成悬空指针。

因此，当满足下面任一情况时，通常应该把字符串内容复制到 pool 中：

1. 数据来自局部缓冲区。
2. 数据来自临时 `std::string`。
3. 数据在当前函数返回后可能失效。
4. PJSIP 后续还要异步使用这些字段。

---

## 用一句话概括

1. `pj_pool_zalloc` 是给“对象实体”分配内存，尤其是指针指向的对象。
2. `desc.fmt[0]` 不需要单独分配，因为它已经包含在 `m` 这块整体内存里。
3. `pj_str_t` 只是“指针 + 长度”的描述结构，不代表它自己拥有字符串内容。
4. `pj_str("96")` 只是引用已有常量字符串，不会额外分配内存。

---

## 对当前代码的直接理解

以 [SipSupService/src/OpenStream.cpp](../../../SipSupService/src/OpenStream.cpp) 这段代码为例：

```cpp
pjmedia_sdp_media* m = (pjmedia_sdp_media*)pj_pool_zalloc(...);
sdp->media[0] = m;
m->desc.fmt_count = 1;
m->desc.fmt[0] = pj_str("96");
```

可以这样理解：

1. 第一行创建了一个完整的 `pjmedia_sdp_media` 对象。
2. `desc` 和 `fmt[]` 都已经在这个对象内部。
3. 后两行只是填写字段，不是在新建 `fmt[0]`。

所以，是否需要 `pj_pool_zalloc`，关键看这个成员是“实体本身”还是“对别的实体的引用”。
