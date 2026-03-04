# L0 → L1 Compaction 流程 Spec

## Why
当前系统在多次 Flush 后会产生多个 L0 层 SSTable 文件，导致查询效率下降（需要遍历多个文件）。需要实现 Compaction 流程，将多个 L0 文件合并为一个 L1 文件，清理重复数据和删除标记，控制磁盘文件数量。

## What Changes
- 在 `KVStore` 中实现 `DoCompaction()` 核心流程
- 实现 L0 文件选择与迭代器创建
- 使用 `CompactionMerger` 进行多路归并
- 使用 `SSTableBuilder` 生成新的 L1 文件
- 实现原子性文件更新（临时文件 + 重命名）
- 删除旧的 L0 文件
- 更新 SSTable Readers 列表

## Impact
- Affected specs: `week-6-compaction-trigger`, `week-6-compaction-merger`
- Affected code: `include/kv_store.h`, `src/kv_store.cpp`

## ADDED Requirements

### Requirement: L0 → L1 Compaction 流程
系统 SHALL 在 L0 文件数量达到阈值时，自动执行 Compaction，将所有 L0 文件合并为一个 L1 文件。

#### Scenario: 基础合并成功
- **GIVEN** 数据目录中存在 4 个 L0 SSTable 文件
- **WHEN** 触发 Compaction
- **THEN** 生成 1 个 L1 SSTable 文件
- **AND** 所有 L0 文件被删除
- **AND** 数据完整可查

#### Scenario: 重复 Key 处理
- **GIVEN** 同一 Key 在多个 L0 文件中存在不同值
- **WHEN** 执行 Compaction
- **THEN** 合并后的 L1 文件只保留最新版本

#### Scenario: Tombstone 处理
- **GIVEN** L0 文件中存在 Tombstone（删除标记）
- **WHEN** 执行 Compaction
- **THEN** Tombstone 被正确保留在 L1 文件中

#### Scenario: 查询不受影响
- **GIVEN** Compaction 正在进行
- **WHEN** 执行 Get 查询
- **THEN** 查询正常工作，返回正确结果

### Requirement: 原子性文件操作
系统 SHALL 使用临时文件机制保证 Compaction 的原子性。

#### Scenario: 写入成功后原子更新
- **GIVEN** Compaction 正在生成 L1 文件
- **WHEN** 文件写入完成
- **THEN** 使用原子重命名操作将临时文件转为正式文件

#### Scenario: 崩溃恢复一致性
- **GIVEN** Compaction 过程中系统崩溃
- **WHEN** 重启恢复
- **THEN** 系统状态一致（要么旧文件仍在，要么新文件生效）

## MODIFIED Requirements

### Requirement: Flush 后触发 Compaction
`KVStore::Flush()` 方法在完成 MemTable 刷盘后，SHALL 检查是否需要执行 Compaction，并在需要时自动调用 `DoCompaction()`。

## REMOVED Requirements
无
