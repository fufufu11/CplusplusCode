# 5. 磁盘存储：SSTable 文件格式

## 5.1 SSTable 概览
SSTable (Sorted String Table) 是 Google Bigtable 论文中提出的核心数据结构，也是 LevelDB/RocksDB 的基础。
它的核心特性是：
1. **有序性**：内部 Key 有序存储，支持二分查找。
2. **不可变性 (Immutable)**：一旦生成，永不修改。更新数据通过生成新的 SSTable 实现，旧文件由 Compaction 清理。

## 5.2 物理布局 (Physical Layout)
为了支持高效的随机读取（Get）和范围查询（Scan），我们将 SSTable 切分为多个 **Block**。
典型的物理布局如下：

```text
+-------------------------+ <--- File Start (Offset 0)
| Data Block 1            |
| [Key1 ... Key100]       |
+-------------------------+
| Data Block 2            |
| [Key101 ... Key200]     |
+-------------------------+
| ...                     |
+-------------------------+
| Data Block N            |
+-------------------------+ <--- Meta Block Offset
| Meta Block (Optional)   |
| (e.g. Bloom Filter)     |
+-------------------------+ <--- Index Block Offset
| Index Block             |
| Key100 -> {Offset,Size} |
| Key200 -> {Offset,Size} |
| ...                     |
+-------------------------+ <--- Footer Offset (File Size - 48)
| Footer (Fixed 48B)      |
| - Meta Index Handle     | ---> Points to Meta Block
| - Index Handle          | ---> Points to Index Block
| - Magic Number          |
+-------------------------+ <--- File End
```

### 5.2.1 核心组件关系图解 (The Big Picture)

为了彻底理解这些名词及其关系，我们可以把 SSTable 看作一本**字典**：

1.  **SSTable File** = **整本字典**。
2.  **Data Block** = **字典的正文页**。
    - **角色**：存肉。这是真正存储用户 Key-Value 数据的地方。
    - 每一页包含了几十个单词（Key）和解释（Value）。
    - 字典太厚了，必须撕成一页一页（Block）来管理，方便按需加载（缓存）。
3.  **Meta Block (附录)**：
    - **角色**：存工具。比如 **Bloom Filter**（布隆过滤器），它可以快速告诉我们"某个 Key 绝不存在于这本字典里"，从而省去翻书的时间。
    - 数量通常只有 1 个（或者没有）。
4.  **Index Block** = **侧边的字母索引标签**（或者目录页）。
    - **角色**：存路标。
    - 它不记单词的具体解释，只记"'Apple' 这个词在第 1 页"，"'Banana' 在第 2 页"。
    - 它的 Key 是每个 Data Block 的**最后一个词**（最大值）。
    - 它的 Value 是 **BlockHandle**（这一页在文件的第几行、有多少字）。
5.  **Footer** = **封底的说明书**。
    - **角色**：存入口。
    - 它是我们拿起这本字典（打开文件）时**唯一**确定的东西（固定在最后）。
    - 它告诉我们："目录页（Index Block）在第 900 页"。
    - 只有读了 Footer，才能找到 Index Block；只有读了 Index Block，才能找到 Data Block。

**引用链 (Reference Chain)**：
`Footer` -> `Index Block` -> `Data Block` -> `User Key/Value`

## 5.3 Data Block 格式
Data Block 是 I/O 的最小单元（通常 4KB）。
内部格式：
```text
+-----------+-----------+-------+-------+
| Entry 1   | Entry 2   | ...   | CRC32 |
+-----------+-----------+-------+-------+
```
每个 Entry (KV) 的格式：
- KeyLen (4B, uint32_t)
- ValueLen (4B, uint32_t)
- ValueType (1B, uint8_t) - 0=NORMAL, 1=TOMBSTONE
- Key Bytes
- Value Bytes

**ValueType 字段说明：**
- `NORMAL = 0`：普通值，Value Bytes 包含实际数据
- `TOMBSTONE = 1`：删除标记，Value Bytes 为空

这个设计避免了使用特殊字符串（如 `"__tombstone__"`）作为删除标记，用户可以存储任意字符串值。

## 5.4 Index Block 与 BlockHandle
Index Block 也是一个 Block，但它存的 Key 是 **每个 Data Block 的最后一个 Key**（即该 Block 的最大值），Value 是 **BlockHandle**。

**什么是 BlockHandle？**
`BlockHandle` 本质上是一个"文件内部指针"。
```cpp
struct BlockHandle {
    uint64_t offset; // 数据块在文件中的起始位置
    uint64_t size;   // 数据块的长度
};
```
它的设计意图是**解耦索引与数据**。Index Block 只需要知道"数据在哪里（Offset）"和"有多大（Size）"，而不需要关心数据的内容。

**查找流程**：
1. 在内存中加载 Index Block。
2. 二分查找目标 Key。
3. 找到第一个 `>= Key` 的索引项，获取其 `BlockHandle`。
4. 根据 `BlockHandle` 的 offset 和 size，去磁盘读取对应的 Data Block。

## 5.5 Footer 格式与设计哲学

**Footer 位于文件末尾，且长度固定（本项目中为 48 字节）。**

**1. 为什么要放在末尾？**
这是 **Immutable（不可变）** 与 **Append-only（追加写）** 策略的必然结果。
- 在写入 SSTable 时，我们是顺序写入 Data Block 的。
- 只有写完所有 Data Block，我们才知道 Index Block 应该包含哪些内容（Offset/Size）。
- 所以 Index Block 必须写在 Data Block 之后。
- 既然 Index Block 最后才写，那谁来记录 Index Block 的位置呢？只能是最后写入的 Footer。
- **读取顺序**：Open 文件 -> Seek 到末尾 -> 读 Footer -> 拿 Index Block Handle -> 读 Index Block -> 准备好查询。

**2. 为什么要固定长度？**
为了方便读取。我们不需要遍历整个文件去找 Footer，只需要 `Seek(file_size - 48)` 就能直接命中它。

**3. 结构详解**
```cpp
struct Footer {
    BlockHandle metaindex_handle; // 指向元数据块（BloomFilter 等）
    BlockHandle index_handle;     // 指向索引块
    uint64_t magic_number;        // 魔数
};
```

**4. 什么是 Magic Number？**
- **定义**：一个固定的 8 字节整数（本项目使用 `0xdb4775248b80fb57`）。
- **来源揭秘**：这个数字并非随机生成。它是 **LevelDB** 项目主页 URL (`http://code.google.com/p/leveldb/`) 的 SHA-1 哈希值的前 64 位。
    - SHA-1: `57fb808b247547db...`
    - Little Endian: `0xdb4775248b80fb57`
    - 这是一种程序员式的幽默与致敬。
- **作用**：
    1.  **身份识别**：确认这个文件真的是 SSTable，而不是一张图片或日志文件。
    2.  **完整性校验**：如果文件尾部被截断，读取到的魔数大概率是对不上的，从而快速发现文件损坏。
