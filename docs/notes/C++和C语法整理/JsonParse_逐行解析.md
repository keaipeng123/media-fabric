# JsonParse 快速说明（够用版）

> 目标：知道 `JsonParse` 每个函数做什么、怎么用、有哪些坑。
>
> 对照位置：`SipSubService/include/Common.h` 的 `class JsonParse`。

---

## 1. 原始代码（对照用）

```cpp
class JsonParse
{
    public:
    JsonParse(string s):m_str(s){}
    JsonParse(Json::Value j):m_json(j){}
    bool toJson(Json::Value& j)
    {
        bool bret =false;
        Json::CharReaderBuilder builder;
        Json::CharReaderBuilder::strictMode(&builder.settings_);
        builder["collectComents"]=true;

        const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());//智能指针
        JSONCPP_STRING errs;
        bret=reader->parse(m_str.data(),m_str.data()+m_str.size(),&j,&errs);
        if(!bret||!errs.empty())
        {
            LOG(ERROR)<<"json parse error:"<<errs.c_str();
        }
        return bret;
    }

    string toString()
    {
        Json::StreamWriterBuilder builder;
        const char* indent="";
        builder["indentation"]=indent;
        return Json::writeString(builder,m_json);
    }
    private:
    string m_str;
    Json::Value m_json;
};
```

---

## 2. 每个函数是做什么的

这个类本质上提供两条路径（注意：互相独立）：

1) **字符串 → JSON（解析）**
- `JsonParse(string s)`：把 JSON 文本保存到 `m_str`。
- `bool toJson(Json::Value& j)`：用 JsonCpp 把 `m_str` 解析到 `j`，成功返回 `true`；失败返回 `false` 并打印 `errs`。
    - 内部启用 `strictMode`（更严格的 JSON 语法）。
    - 代码里  `builder["collectComments"]`（区分大小写）。另外 `collectComments` 在 `allowComments=false` 时会被忽略（严格模式通常会关掉注释）。

**`toJson` 里关键语句在做什么（够用理解）**
- `bool bret = false;`：先默认失败，后面用解析结果覆盖它。
- `Json::CharReaderBuilder builder;`：准备解析器的配置对象。
- `Json::CharReaderBuilder::strictMode(&builder.settings_);`：把配置切到严格模式的一组默认值。
- `builder["collectComments"] = true;`：尝试“收集注释”；但如果 `allowComments=false`，该项会被忽略。
- `unique_ptr<CharReader> reader(builder.newCharReader());`：根据配置创建解析器，用智能指针自动释放。
- `JSONCPP_STRING errs;`：接收解析失败时的错误详情。
- `reader->parse(begin, end, &j, &errs)`：把 `m_str` 解析进 `j`，返回值表示成功/失败，同时填充 `errs`。
- `if (!bret || !errs.empty()) LOG(ERROR) ...`：失败或有错误信息就打印日志，方便定位输入 JSON 问题。

**CharReaderBuilder 的默认配置长什么样（当前环境打印结果）**

说明：这份配置来自 `Json::CharReaderBuilder builder;` 后直接打印 `builder.settings_`。不同 JsonCpp 版本默认值可能不同，以你实际打印为准。

```json
{
    "allowComments" : false,
    "allowDroppedNullPlaceholders" : false,
    "allowNumericKeys" : false,
    "allowSingleQuotes" : false,
    "allowSpecialFloats" : false,
    "collectComments" : true,
    "failIfExtra" : true,
    "rejectDupKeys" : true,
    "stackLimit" : 1000,
    "strictRoot" : true
}
```

小提醒：这里 `allowComments=false` 但 `collectComments=true` 同时出现并不矛盾；JsonCpp 会在不允许注释时忽略 `collectComments`（也就是“收集注释”不会生效）。

2) **JSON → 字符串（序列化）**
- `JsonParse(Json::Value j)`：把 JSON 对象保存到 `m_json`。
- `string toString()`：把 `m_json` 序列化成字符串返回；`indentation` 设为 `""`，所以输出是紧凑单行 JSON。

**`toString` 里关键语句在做什么（够用理解）**
- `Json::StreamWriterBuilder builder;`：准备“怎么输出 JSON 字符串”的配置对象。
- `builder["indentation"] = "";`：设置缩进为空串，输出变成紧凑单行；如果换成比如 `"  "` 就会变成更易读的多行缩进格式。
- `Json::writeString(builder, m_json)`：按配置把 `m_json` 转成 `string` 并返回。

---

## 3. 用法示例

### 3.1 字符串 -> JSON

```cpp
JsonParse parser("{\"id\":1}");
Json::Value root;
if (parser.toJson(root)) {
    // root["id"] == 1
}
```

### 3.2 JSON -> 字符串

```cpp
Json::Value root;
root["id"] = 1;
JsonParse parser(root);
string s = parser.toString();
```

---

