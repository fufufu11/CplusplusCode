# Compaction 触发机制 Spec

## Why
当前系统在多次 Flush 后会产生大量 L0 层 SSTable 文件，导致查询效率下降（需要遍历多个文件）。需要实现 Compaction 触发机制，在 L0 文件数量达到阈值时自动触发合并操作。

## What Changes
- 新增 `CompactionConfig` 配置结构体，定义触发阈值等参数
- 新增 `ShouldCompact()` 判断函数，检查是否需要触发 Compaction
- 在 `KVStore::Flush()` 后调用触发检查
- 新增 `GetL0FileCount()` 辅助函数，统计 L0 层文件数量

## Impact
- Affected specs: `kv_store.h`
- Affected code: `include/kv_store.h`

## ADDED Requirements

### Requirement: Compaction 触发判断
系统 SHALL 提供 `ShouldCompact()` 函数，用于判断是否需要触发 Compaction。

#### Scenario: L0 文件数量达到阈值
- **WHEN** L0 层 SSTable 文件数量 ≥ 4（可配置）
- **THEN** `ShouldCompact()` 返回 `true`

#### Scenario: L0 文件数量未达阈值
- **WHEN** L0 层 SSTable 文件数量 < 4
- **THEN** `ShouldCompact()` 返回 `false`

### Requirement: 触发时机
系统 SHALL 在每次 Flush 操作完成后检查是否需要触发 Compaction。

#### Scenario: Flush 后自动检查
- **WHEN** `Flush()` 操作完成
- **THEN** 系统自动调用 `ShouldCompact()` 检查

### Requirement: 可配置阈值
系统 SHALL 支持配置 Compaction 触发阈值。

#### Scenario: 默认阈值
- **WHEN** 用户未指定配置
- **THEN** L0 触发阈值默认为 4

#### Scenario: 自定义阈值
- **WHEN** 用户通过 `CompactionConfig` 设置 `l0_trigger_count = 8`
- **THEN** L0 文件数量 ≥ 8 时才触发 Compaction

### Requirement: L0 文件统计
系统 SHALL 能够正确统计 L0 层 SSTable 文件数量。

#### Scenario: 统计 L0 文件
- **WHEN** 调用 `GetL0FileCount()`
- **THEN** 返回文件名以 `L0_` 开头的 `.sst` 文件数量
