# 随机数生成：给初学者的速记（`rand/srand`、`random/srandom`、C++11 `<random>`）

在项目里（例如 `GlobalCtl::randomNum(int length)`）我们经常需要生成“看起来随机”的值：SIP 的 `nonce/opaque/tag/branch/call-id` 等。

先记住一句话：

> 计算机里大多数“随机数”其实是**伪随机**：给定同一个“种子 seed”，输出序列就会重复。

---

## 1. 两套常见函数：谁和谁是一对？

你现在工程里用到了 `random()`，并且在 `main()` 里已经改成了 `srandom(time(0))`，这是一对。

| 生成函数 | 设置种子（seed） | 备注 |
|---|---|---|
| `rand()` | `srand(seed)` | C 标准库常见组合 |
| `random()` | `srandom(seed)` | 类 POSIX/glibc 常见组合（返回值通常是 `long`） |

补充小知识：

- `rand()` 常见范围是 `[0, RAND_MAX]`（`RAND_MAX` 平台相关，至少 32767）
- `random()` 通常返回非负的 `long`

重点：

- `srand()` **不会**影响 `random()`；`srandom()` **也不会**影响 `rand()`。

---

## 2. 为什么“同一秒会重复”？

如果你用 `time(0)`/`time(NULL)` 当种子，它的精度是“秒”。

- 同一秒启动两次程序 → 种子相同 → 随机序列很可能相同
- 解决思路：播种只做一次（放在 `main()`），不要在高频函数里反复播种

你现在在 `main()` 里播种一次是对的。

---

## 3. 你这个 `randomNum()` 写法的关键坑（务必避开）

### 3.1 `%15` 写错了：会少一种值

如果你想生成 16 进制的一位（`0..f`），范围应该是 0~15（16 种）。

- `random() % 15` 只会得到 0~14，永远不会出现 `f`
- 应该写成 `% 16`

### 3.2 `%` 取模会有“分布不均”的问题

很多教程会写：

- `a + rand() % n` 生成区间

这能用，但严格来说可能不完全均匀（取模偏差）。初学阶段可以先理解为：

- `n` 很小（比如 16）时影响不大
- 想要更“均匀”和更“可控”，优先用 C++11 `<random>` 的 `uniform_int_distribution`

### 3.3 多线程注意：`rand/random` 是全局状态

`rand()`/`random()` 会共享内部状态，多线程下可能互相干扰（甚至需要锁或用 `random_r()` 一类的线程安全版本）。

---

## 4. 更推荐的方式：C++11 `<random>`（更好控制范围）

核心用法很简单：

> `value = distribution(engine)`

建议（服务端常用）：

- 引擎只初始化一次：`static thread_local`
- 用 `uniform_int_distribution` 产生你要的范围（比如 0~15）

---

## 5. 工程里该怎么选？（一句话版）

- 只是想要“低碰撞/看起来随机”的字符串：`<random>` 很合适
- 用 `rand/random` 也行，但要确保：
  - 播种函数匹配（`random()`↔`srandom()`、`rand()`↔`srand()`）
  - 16 进制一位用 `%16`
- 如果需求是“安全不可预测”（密钥/令牌级别）：不要依赖 `rand/random/mt19937`，应使用系统级 CSPRNG（如 `/dev/urandom` 等方案）

---

## 6. 结合本项目（SipSupService）举例：这套随机数到底怎么走的？

### 6.1 你项目里“播种 seed”发生在哪里？

在 main 入口处播种一次：

- `srandom(time(0));`：见 SipSupService/src/main.cpp 第 74 行

这意味着：

- 从这一刻开始，后续所有 `random()` 的输出序列都会基于这个 seed 继续往下走
- 如果你在同一秒内启动两次程序，两次 seed 可能相同 → 两个进程的 `random()` 序列可能相同

如果你希望“同一秒启动也尽量不一样”（更稳一点），可以把 seed 混入进程号（思路示例）：

```cpp
// 需要：#include <unistd.h>
srandom((unsigned)time(0) ^ (unsigned)getpid());
```

### 6.2 你项目里“生成随机字符串”的函数长什么样？

你当前实现：SipSupService/src/GlobalCtl.cpp 第 110 行开始：

- 每循环一次：`value = random() % 16` → 得到 0~15
- `ss << std::hex << value` → 以 16 进制写入一位字符（`0..f`）
- 循环 `length` 次 → 最终得到一个长度约等于 `length` 的 hex 字符串

所以：

- `GlobalCtl::randomNum(8)` 可能长这样：`0a3f9c1b`
- `GlobalCtl::randomNum(32)` 就是 32 个 hex 字符 ≈ 128 bit 随机值

### 6.3 这些随机串在 SIP 里用在哪里？

在处理注册鉴权（401）时，你用它生成 `nonce`/`opaque`：

- nonce：SipSupService/src/SipRegister.cpp 第 150 行
- opaque：SipSupService/src/SipRegister.cpp 第 165 行

在普通 REGISTER 处理里也生成了一个随机串用于日志打印：

- random：SipSupService/src/SipRegister.cpp 第 251 行

把笔记里的概念对应到这里就是：

- **同一个进程内**：每次调用 `GlobalCtl::randomNum(32)`，都会从同一条 `random()` 序列继续取数，所以一般不会“每次都一样”
- **不同进程之间**：如果 seed 一样（例如同一秒启动），那序列可能一样 → nonce/opaque 也可能一样（这就是“同一秒重复”的真实来源）

### 6.4 这套实现的优缺点（结合你的代码说人话）

- 优点：简单、能用、速度快；你已经把 `%15` 修成 `%16`，不会漏掉 `f`
- 缺点：
  - `time(0)` 只有秒级精度，多进程同秒启动可能重复
  - `random()` 是全局状态，多线程高并发下可能互相影响
  - 不是“安全不可预测”的随机数（如果你把它当作安全令牌用，就不够）

---

## 7. 你提到的这段 C++11 `<random>` 写法，也可以这样理解

你项目里 `GlobalCtl::randomNum()` 曾经有一版（目前被 `#if 0` 注释掉）的实现大概是：

```cpp
// 随机数种子来源（尽量来自系统熵源）
std::random_device rd;

// 伪随机引擎（给它一个 seed）
std::mt19937 gen(rd());

// “分布”：让输出落在 0~15（16 进制一位）
std::uniform_int_distribution<> dis(0, 15);

std::stringstream ss;
for (int i = 0; i < length; ++i) {
    int value = dis(gen);      // 关键：distribution(engine)
    ss << std::hex << value;   // 输出 0..f
}
return ss.str();
```

把它翻译成人话就是：

- `random_device`：尽量从系统拿“更随机”的 seed（实现相关）
- `mt19937`：一个很常用、很快的伪随机数生成器
- `uniform_int_distribution(0,15)`：保证每次得到 0~15（不会漏掉 `f`）
- `dis(gen)`：记住这个固定套路：`value = distribution(engine)`

注意点（和你当前 `random()` 方案对比）：

- 这段代码如果“每次调用 randomNum 都重新构造 `mt19937`”，会比复用引擎更慢
- 更常见的写法是：把引擎做成 `static thread_local`，每个线程初始化一次（性能更好，也更适合多线程）
