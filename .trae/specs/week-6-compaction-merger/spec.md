# Multi-way Merge Sort for Compaction Spec

## Why
LSM-Tree 的 Compaction 需要将多个 L0 层 SSTable 文件合并为 L1 层文件。多路归并排序是 Compaction 的核心算法，用于将多个有序输入流合并为一个有序输出流，同时处理重复 Key 和 Tombstone。

## What Changes
- 新增 `CompactionMerger` 类，实现多路归并排序算法
- 使用最小堆（Min-Heap）维护多个迭代器的当前最小 Key
- 支持流式处理，避免一次性加载所有数据到内存
- 正确处理重复 Key（保留最新版本）和 Tombstone

## Impact
- Affected specs: Compaction 机制
- Affected code: 新增 `include/compaction_merger.h`、`src/compaction_merger.cpp`

## ADDED Requirements

### Requirement: CompactionMerger 类设计
系统应提供 `CompactionMerger` 类，用于合并多个 SSTable 迭代器。

#### Scenario: 构造与初始化
- **WHEN** 用户传入多个 SSTable 迭代器
- **THEN** 系统应初始化最小堆，将每个迭代器的第一个元素加入堆中

#### Scenario: 流式遍历
- **WHEN** 用户调用 `Next()` 方法
- **THEN** 系统应弹出堆顶元素（最小 Key），并将该迭代器的下一个元素加入堆中

### Requirement: 重复 Key 处理
当多个迭代器指向同一个 Key 时，系统应正确处理版本优先级。

#### Scenario: 普通值覆盖
- **GIVEN** 两个 SSTable 中存在相同 Key，但 Value 不同
- **WHEN** 执行归并
- **THEN** 系统应保留来自较新 SSTable 的值（迭代器顺序决定优先级）

#### Scenario: 删除后无新值
- **GIVEN** Key 在旧 SSTable 中存在，在新 SSTable 中被标记为 Tombstone
- **WHEN** 执行归并
- **THEN** 系统应输出 Tombstone（后续写入 SSTable 时保留）

#### Scenario: 删除后有新值
- **GIVEN** Key 在中间 SSTable 中被标记为 Tombstone，在最新 SSTable 中有新值
- **WHEN** 执行归并
- **THEN** 系统应输出新值

### Requirement: 迭代器接口
`CompactionMerger` 应提供与 SSTableReader::Iterator 一致的接口。

#### Scenario: 标准迭代器操作
- **WHEN** 用户使用 `Valid()`、`Key()`、`Value()`、`Next()` 方法
- **THEN** 系统应正确返回当前状态和数据

### Requirement: 内存效率
归并过程应保持低内存占用。

#### Scenario: 大数据集归并
- **GIVEN** 4 个 SSTable，每个包含 10000 条记录
- **WHEN** 执行归并
- **THEN** 内存占用应仅与迭代器数量成正比，而非总数据量
