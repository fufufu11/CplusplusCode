# Tasks

- [x] Task 1: 定义 CompactionConfig 配置结构体
  - [x] SubTask 1.1: 在 `kv_store.h` 中定义 `CompactionConfig` 结构体
  - [x] SubTask 1.2: 包含 `l0_trigger_count` 成员（默认值 4）
  - [x] SubTask 1.3: 在 `KVStore` 构造函数中接收可选的 `CompactionConfig` 参数

- [x] Task 2: 实现 GetL0FileCount() 辅助函数
  - [x] SubTask 2.1: 在 `KVStore` 中添加 `GetL0FileCount()` 私有方法
  - [x] SubTask 2.2: 遍历 `sstable_readers_`，统计文件名以 `L0_` 开头的数量
  - [x] SubTask 2.3: 添加 `GetL0FileCount()` 公共方法用于测试

- [x] Task 3: 实现 ShouldCompact() 判断函数
  - [x] SubTask 3.1: 在 `KVStore` 中添加 `ShouldCompact()` 私有方法
  - [x] SubTask 3.2: 比较 `GetL0FileCount()` 与 `config_.l0_trigger_count`
  - [x] SubTask 3.3: 返回布尔值表示是否需要触发 Compaction

- [x] Task 4: 在 Flush 后集成触发检查
  - [x] SubTask 4.1: 在 `Flush()` 方法末尾调用 `ShouldCompact()`
  - [x] SubTask 4.2: 如果返回 `true`，输出日志提示需要 Compaction（本周暂不实现实际合并）

- [x] Task 5: 编写单元测试
  - [x] SubTask 5.1: 测试 `GetL0FileCount()` 正确统计 L0 文件
  - [x] SubTask 5.2: 测试 `ShouldCompact()` 在阈值以下返回 `false`
  - [x] SubTask 5.3: 测试 `ShouldCompact()` 达到阈值返回 `true`
  - [x] SubTask 5.4: 测试自定义阈值配置生效

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 2
- Task 4 depends on Task 3
- Task 5 depends on Task 4
