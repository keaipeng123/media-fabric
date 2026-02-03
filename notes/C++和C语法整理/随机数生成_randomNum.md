# 随机数生成：randomNum 笔记（C++11 `<random>`）

在工程里（例如 `GlobalCtl::randomNum(int length)`）常见的需求是：生成一段“看起来随机”的字符串，用于 `tag`/`branch`/`nonce`/`call-id` 等。

C++11 起推荐使用 `<random>` 里的**随机数引擎（engine）+ 分布（distribution）**模型：

- **引擎**：负责产生伪随机序列（如 `std::mt19937`、`std::mt19937_64`）
- **分布**：把引擎输出映射到指定范围/形态（如均匀分布 `std::uniform_int_distribution`）

> 简单记法：`value = distribution(engine)`

---

## 1. 为什么不推荐 `rand()` / `random()`

- `rand()` 质量一般、范围小（`RAND_MAX` 平台相关），很多实现周期/低位随机性较差
- 常见写法 `rand()%N` 会产生**取模偏差**（除非 `RAND_MAX+1` 能整除 `N`）
- `srand(time(nullptr))` 在短时间多次调用时容易重复
- 线程安全与可控性差

在服务端（SIP/信令等）需要“更稳定、更可控、更均匀”的随机行为时，优先 `<random>`。

---

## 2. `random_device`、`mt19937`、`uniform_int_distribution` 是什么

### 2.1 `std::random_device`

- 用来获取**非确定性**随机源（如果实现/平台支持）
- 也可能退化为伪随机（实现相关），但一般可以当作“播种用的种子来源”

### 2.2 `std::mt19937`

- Mersenne Twister 伪随机引擎
- 周期非常长、性能好，适合大多数业务场景
- **不适合密码学安全**（需要抗预测的场景不要用它当唯一随机源）

### 2.3 `std::uniform_int_distribution<int>(a,b)`

- 生成均匀分布的整数，范围闭区间 `[a,b]`
- 例如 `uniform_int_distribution<>(0, 15)` 生成 0~15，用于十六进制字符

---

## 3. 你当前 `randomNum(length)` 的逻辑解读

典型实现（概念上）：

- `random_device rd;` 获取种子
- `mt19937 gen(rd());` 用种子初始化引擎
- `uniform_int_distribution<>(0,15)` 每次生成一位 0~15
- `std::hex` 以十六进制写入 `stringstream`，得到长度为 `length` 的 hex 字符串

这种写法的输出：

- `length == 8` → 类似 `0a3f9c1b`
- 0~9 输出字符 `'0'..'9'`，10~15 输出 `'a'..'f'`

---

## 4. 常见坑与改进建议

### 4.1 不要每次调用都重新构造引擎

当前每次调用 `randomNum()` 都构造：`random_device`、`mt19937`。

- 这会增加开销
- 在高并发/高频调用场景不划算

**更常见做法**：让引擎静态复用，且线程隔离：`static thread_local`。

### 4.2 `std::hex` 是“流状态”，会一直生效

`ss << std::hex << value;` 会把流设置成十六进制格式，后续输出都将是 hex。

- 你这里从头到尾都在输出 hex，没问题
- 如果一个 `stringstream` 里还要输出十进制，记得用 `std::dec` 切回来

### 4.3 “长度”到底是字符数还是字节数

你现在生成的是 **hex 字符串**：

- 生成 `length` 次，每次写 0~15 的 1 个 hex 字符
- 所以结果字符串长度大概率就是 `length`

但如果你要生成**随机字节**再编码为 hex：

- 1 个字节会变成 2 个 hex 字符
- 例如 16 字节 nonce → 32 个 hex 字符

### 4.4 这不是“密码学安全随机数”

- `mt19937` 可被预测（在攻击者能观察足够输出时）
- 若用于鉴权/安全令牌/会话密钥，建议使用系统级 CSPRNG（不同平台不同方案）

业务里常见的 SIP `branch`/`tag` 多数是“唯一性/低碰撞”需求，通常 `<random>` 足够；但“安全性”需求要另行评估。

---

## 5. 推荐实现：生成 N 位十六进制字符串（高性能/线程安全）

```cpp
#include <random>
#include <sstream>
#include <string>

// 生成 n 个 hex 字符（每个字符对应 0~15）
inline std::string random_hex(std::size_t n)
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    static thread_local std::uniform_int_distribution<int> dis(0, 15);

    std::ostringstream oss;
    oss.setf(std::ios::hex, std::ios::basefield);

    for (std::size_t i = 0; i < n; ++i)
    {
        oss << dis(gen);
    }
    return oss.str();
}
```

特点：

- `thread_local`：每个线程独立一个引擎/分布，避免锁竞争
- 引擎只初始化一次：性能更好

---

## 6. 生成“字节级随机数”并编码为 hex（每字节 2 个字符）

```cpp
#include <iomanip>
#include <random>
#include <sstream>
#include <string>

// 生成 n_bytes 个随机字节，并转成 2*n_bytes 个 hex 字符
inline std::string random_hex_bytes(std::size_t n_bytes)
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    static thread_local std::uniform_int_distribution<int> dis(0, 255);

    std::ostringstream oss;
    oss.setf(std::ios::hex, std::ios::basefield);

    for (std::size_t i = 0; i < n_bytes; ++i)
    {
        const int byte = dis(gen);
        oss << std::setw(2) << std::setfill('0') << byte;
    }
    return oss.str();
}
```

适合：

- 希望“熵更足”，且不想纠结 hex 位数
- 常见：随机 16 字节 → 32 位 hex

---

## 7. 生成指定范围的整数（示例）

```cpp
#include <random>

inline int random_int(int lo, int hi)
{
    static thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dis(lo, hi);
    return dis(gen);
}
```

---

## 8. 工程实践建议（服务端场景）

- **高频调用**：复用引擎（`static thread_local`）
- **可复现测试**：用固定 seed（例如从配置读取）
- **唯一性优先**：随机串 + 时间戳/自增序列/设备ID 混合，减少碰撞概率
- **安全敏感**：不要用 `mt19937` 当唯一随机源（需平台级 CSPRNG）

---

## 9. 跟当前代码的对应关系（写笔记时的结论）

你现有 `GlobalCtl::randomNum(length)` 属于：

- 十六进制字符级随机串（每次生成 0~15）
- 满足一般业务的 nonce/tag/branch 需求

如果后续你发现它在高频调用下性能不理想，优先把“每次构造引擎”改为“静态复用引擎”。
