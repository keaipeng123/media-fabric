# XML 学习笔记（围绕 `XmlParser`：tinyxml2 的轻量封装）

> 对应工程代码：
>
> - `SipSubService/include/XmlParser.h`
> - `SipSubService/src/XmlParser.cpp`
> - 典型使用点：`SipSubService/src/SipHeartBeat.cpp`（构造 GB28181 `keepalive` 的 XML Body）
>
> 目标：
>
> 1) 搞清 `XmlParser` 封装了 tinyxml2 的哪些能力。
> 2) 学会用它快速拼出 GB28181 `MESSAGE`/`MANSCDP+xml` 的 XML 内容。
> 3) 识别当前实现中的内存与安全边界（避免踩坑）。

---

## 0. 这玩意儿解决什么问题？

在 GB28181 里，经常需要在 SIP `MESSAGE` 的 body 里带 `MANSCDP+xml`，例如：

- 心跳通知（`<Notify>...<CmdType>keepalive</CmdType>...</Notify>`）
- 目录查询响应、设备信息、报警通知等

如果手写字符串拼接：

- 易错（漏标签、少换行、转义问题）
- 可维护性差（字段增删容易破坏结构）

`XmlParser` 通过 tinyxml2 提供：

- 创建根节点
- 在节点下插入子节点并写入文本值
- 给节点添加属性（attribute）
- 打印整个 XML 到外部缓冲区

---

## 1. `XmlParser` 类结构一眼看懂

### 1.1 成员变量

- `tinyxml2::XMLDocument* m_document`：tinyxml2 的文档对象（所有节点都由它创建并管理）
- `tinyxml2::XMLElement* m_rootElement`：根节点指针
- `char* m_xmlTitle`：XML 声明（当前写死 `<?xml version="1.0"?>\n`，长度上限 `max_title=64`）

### 1.2 构造/析构做了什么

- 构造：`new XMLDocument()`，初始化根节点为空；分配 `m_xmlTitle` 并写入 `XMLTITLE`
- 析构：`delete m_document`（释放 document 时，tinyxml2 会一并释放它创建/挂接的节点）

> 关键点：你不需要也不应该手动 delete `XMLElement*`。

---

## 2. API 逐个拆解

### 2.1 `AddRootNode(const char* rootName)`

用途：创建根元素并挂到 document 上。

- `m_rootElement = m_document->NewElement(rootName);`
- `m_document->LinkEndChild(m_rootElement);`

返回：根节点 `XMLElement*`（后续插入子节点要用）

### 2.2 `InsertSubNode(XMLElement* parentNode, const char* itemName, const char* value)`

用途：在 `parentNode` 下插入子元素 `<itemName>value</itemName>`。

实现要点：

- `NewElement(itemName)` 创建元素
- `LinkEndChild(insertNode)` 挂到 parent
- 如果 `strlen(value) != 0` 则创建 `XMLText` 并挂到子元素

返回：新创建的子节点指针（可以继续往下挂更深层结构）

### 2.3 `SetNodeAttributes(XMLElement* node, char* attrName, char* attrValue)`

用途：给节点加属性，例如：`<Item code="123" />`。

- `node->SetAttribute(attrName, attrValue);`

### 2.4 `getXmlData(char* xmlBuf)`

用途：把整个 document 打印到 `xmlBuf`。

- `XMLPrinter printer; m_document->Accept(&printer);`
- 先把 `m_xmlTitle` 复制到 `xmlBuf`（最多 `max_title` 字节）
- 再 `strcat(xmlBuf, printer.CStr())` 拼接正文

> 注意：`xmlBuf` 必须由调用方分配足够大空间。

---

## 3. 典型用法：构造 GB28181 心跳 `keepalive` XML

工程里在 `SipSubService/src/SipHeartBeat.cpp` 有真实例子：

```cpp
XmlParser parse;
auto* root = parse.AddRootNode("Notify");
parse.InsertSubNode(root, "CmdType", "keepalive");
parse.InsertSubNode(root, "SN", strIndex);
parse.InsertSubNode(root, "DeviceID", GBOJ(gConfig)->sipId().c_str());
parse.InsertSubNode(root, "Status", "OK");

char xmlbuf[1024] = {0};
parse.getXmlData(xmlbuf);
```

最终 `xmlbuf` 类似：

```xml
<?xml version="1.0"?>
<Notify>
    <CmdType>keepalive</CmdType>
    <SN>...</SN>
    <DeviceID>...</DeviceID>
    <Status>OK</Status>
</Notify>
```

然后把它作为：

- `Content-Type: Application/MANSCDP+xml`
- body 文本内容

---

## 4. 常见坑（结合当前实现）

### 4.1 `xmlBuf` 的长度没有传入：有溢出风险

`getXmlData(char* xmlBuf)` 内部会：

- 先 `strncpy(xmlBuf, m_xmlTitle, max_title)`
- 再 `strcat(xmlBuf, printer.CStr())`

如果 `xmlBuf` 不够大，会发生缓冲区溢出。**目前接口没有任何长度参数**，所以调用方必须自觉。

建议：在调用点用足够大的固定数组（如 2KB/4KB），或改造接口为 `getXmlData(char* buf, size_t cap)` / 直接返回 `std::string`。

### 4.2 `InsertSubNode` 对 `value==nullptr` 不安全

当前写法直接 `strlen(value)`：如果外部传入 `nullptr` 会崩溃。

约定：调用处保证 `value` 非空；或者改造实现先判空。

### 4.3 `m_xmlTitle` 的初始化用 `memcpy(..., max_title)` 有越界读取风险

`XMLTITLE` 是短字符串常量，但 `memcpy(m_xmlTitle, XMLTITLE, max_title)` 会强行读满 64 字节；严格来说这可能读取到字符串字面量之后的内存区域（未定义行为）。

更安全的做法是：`strncpy`/`snprintf` 按实际长度写入。

### 4.4 使用点里的 `new char[1024]` 没有释放（内存泄漏）

`SipHeartBeat.cpp` 示例里：

```cpp
char* xmlbuf = new char[1024];
...
parse.getXmlData(xmlbuf);
```

如果后续没有 `delete[] xmlbuf;`，就会泄漏。更推荐：

- 用栈内存：`char xmlbuf[1024] = {0};`
- 或 `std::string`（如果接口改造为返回 string）

---

## 5. 实战建议：如何把它用“稳”

- **优先栈上创建**：`XmlParser parse;`（析构自动释放 document）
- **缓冲区要留够**：常见 GB28181 XML 小于 1KB，但目录响应/大量 `Item` 可能远大于 1KB
- **避免 `nullptr`**：传参时保证 `itemName/value` 合法
- **需要属性时**：先 `InsertSubNode` 得到节点，再 `SetNodeAttributes`

---

## 6. 可选改造方向（等你要“工程化”时再做）

如果后面要更稳更易用，可以考虑：

1) `getXmlData` 改为返回 `std::string`（避免外部缓冲区与 `strcat`）
2) 全面 `const` 化参数（`const char*`）
3) `InsertSubNode` 支持 `value==nullptr` 等价于“无文本节点”
4) XML 声明可配置（例如声明 encoding：`gb2312/utf-8`）

---

## 7. 你可以用它扩展哪些 GB28181 消息？

把 `<Notify>` 换成不同根节点/字段，就可以生成：

- 设备目录响应（`<Response><CmdType>Catalog</CmdType>...</Response>`）
- 设备信息响应（`<Response><CmdType>DeviceInfo</CmdType>...</Response>`）
- 报警通知（`<Notify><CmdType>Alarm</CmdType>...</Notify>`）

核心套路不变：

1) `AddRootNode`
2) 多次 `InsertSubNode`
3) `getXmlData` 获取字符串
4) 填入 `pjsip_msg_body_create(..., "Application", "MANSCDP+xml", ...)`
