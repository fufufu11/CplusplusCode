# Tasks

- [x] Task 1: 文档化 SSTable 格式 (AI 负责)
    - [x] SubTask 1.1: 在 `docs/DistributedKV_Guide/Learning_Manual.md` 中新增章节（Chapter 5: 磁盘存储与 SSTable），并调整后续章节序号。
    - [x] SubTask 1.2: 详细描述 Data Block 的格式（Entry 结构、Checksum）。
    - [x] SubTask 1.3: 详细描述 Index Block 的格式（Key + BlockHandle）。
    - [x] SubTask 1.4: 详细描述 Footer 的格式（Fixed Length, Magic Number）。

- [x] Task 2: 定义 C++ 核心结构体 (用户负责)
    - [x] SubTask 2.1: 用户手动创建 `include/sstable.h`。
    - [x] SubTask 2.2: 用户根据手册和 AI 提供的参考代码，输入 `struct BlockHandle` 和 `struct Footer` 的定义。
    - [x] SubTask 2.3: AI 检查 `include/sstable.h` 是否存在并正确定义。

# Task Dependencies
- Task 2 depends on Task 1 (代码实现需遵循文档设计)。
