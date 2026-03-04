# Tasks

- [x] Task 1: 设计 DoCompaction() 方法框架
  - [x] SubTask 1.1: 在 `KVStore` 中声明 `DoCompaction()` 私有方法
  - [x] SubTask 1.2: 定义返回值类型（void，失败时抛异常或记录日志）
  - [x] SubTask 1.3: 在 `Flush()` 中调用 `DoCompaction()`（当 `ShouldCompact()` 返回 true）

- [x] Task 2: 实现 L0 文件选择逻辑
  - [x] SubTask 2.1: 创建 `SelectL0Files()` 方法，收集所有 L0 文件路径
  - [x] SubTask 2.2: 为每个 L0 文件创建 `SSTableReader::Iterator`
  - [x] SubTask 2.3: 将迭代器存入 `std::vector<std::unique_ptr<SSTableReader::Iterator>>`

- [x] Task 3: 实现多路归并与 L1 文件生成
  - [x] SubTask 3.1: 创建 `CompactionMerger` 实例，传入所有 L0 迭代器
  - [x] SubTask 3.2: 创建临时 L1 文件路径（如 `L1_temp_001.sst`）
  - [x] SubTask 3.3: 使用 `SSTableBuilder` 遍历 `CompactionMerger`，写入新 SSTable
  - [x] SubTask 3.4: 调用 `builder.Finish()` 完成文件写入并刷盘

- [x] Task 4: 实现原子性文件更新
  - [x] SubTask 4.1: 将临时文件重命名为正式 L1 文件（如 `L1_001.sst`）
  - [x] SubTask 4.2: 更新 `sstable_readers_` 列表（移除 L0，添加 L1）
  - [x] SubTask 4.3: 删除旧的 L0 文件
  - [x] SubTask 4.4: 更新 `next_sstable_id_` 计数器

- [x] Task 5: 实现查询兼容性
  - [x] SubTask 5.1: 确保 Compaction 过程中 `get()` 方法正常工作
  - [x] SubTask 5.2: 考虑在 Compaction 前后保持查询顺序正确

- [x] Task 6: 编写单元测试
  - [x] SubTask 6.1: 测试用例 `BasicCompaction`：4 个 L0 文件合并为 1 个 L1 文件，验证数据完整
  - [x] SubTask 6.2: 测试用例 `CompactionDuplicateKey`：同一 Key 在多个 L0 文件中存在，验证保留最新版本
  - [x] SubTask 6.3: 测试用例 `CompactionTombstonePreserved`：验证 Tombstone 正确保留
  - [x] SubTask 6.4: 测试用例 `CompactionFileCount`：验证 Compaction 后 L0 文件数量减少
  - [x] SubTask 6.5: 测试用例 `CompactionRecovery`：Compaction → 重启 → 验证查询正确
  - [x] SubTask 6.6: 测试用例 `QueryDuringCompaction`：Compaction 过程中查询正常工作

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 2
- Task 4 depends on Task 3
- Task 5 depends on Task 4
- Task 6 depends on Task 5
