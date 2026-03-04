# Week 5 Task 1-2: 读路径分层架构与 Get 查询链路 Spec

## Why
第 5 周的核心目标是完善读路径，实现完整的 Get 查询链路。任务一和任务二分别是理论学习与代码实现，需要确保查询顺序正确、版本可见性正确。

## What Changes
- 任务一：标记为已完成（理论学习）
- 任务二：验证 Get 查询链路实现正确性
  - 确保查询顺序：MemTable → SSTable（从新到旧）
  - 确保找到第一个匹配的 Key 立即返回
  - 补充必要的单元测试验证

## Impact
- Affected specs: Week 5 查询路径任务
- Affected code:
  - `include/kv_store.h` (已实现)
  - `tests/kv_store_test.cpp` (可能需要补充测试)
  - `tests/flush_integration_test.cpp` (已有相关测试)

## ADDED Requirements

### Requirement: Get 查询链路顺序正确
系统应按照正确的顺序查询数据：MemTable → SSTable（从新到旧）。

#### Scenario: MemTable 优先于 SSTable
- **GIVEN** Key A 在 SSTable 中存在值为 "v1"
- **AND** MemTable 中 Key A 的值为 "v2"
- **WHEN** 用户执行 `get(A)`
- **THEN** 返回 "v2"（MemTable 优先）

#### Scenario: 多 SSTable 从新到旧查询
- **GIVEN** Key A 在 L0_001.sst 中存在值为 "v1"
- **AND** Key A 在 L0_002.sst 中存在值为 "v2"（L0_002 更新）
- **WHEN** 用户执行 `get(A)`
- **THEN** 返回 "v2"（更新的 SSTable 优先）

### Requirement: 找到第一个匹配立即返回
查询时找到第一个匹配的 Key 应立即返回，无需继续查找。

#### Scenario: 找到后停止查找
- **GIVEN** Key A 在 MemTable 中存在
- **WHEN** 用户执行 `get(A)`
- **THEN** 返回 MemTable 中的值，不查询 SSTable

## MODIFIED Requirements

### Requirement: 项目计划文档更新
更新 `docs/DistributedKV_Guide/chapters/13-项目计划周级细化.md`：
- 标记任务一为已完成
- 更新任务二的状态

## REMOVED Requirements
无。
