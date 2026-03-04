# Tasks

- [x] Task 1: 设计 CompactionMerger 类接口
  - [x] SubTask 1.1: 在 `include/compaction_merger.h` 中定义 `CompactionMerger` 类
  - [x] SubTask 1.2: 定义迭代器包装结构 `IteratorEntry`（包含迭代器指针、优先级、当前 Key/Value）
  - [x] SubTask 1.3: 声明公共接口：`Valid()`、`Key()`、`Value()`、`Next()`
  - [x] SubTask 1.4: 声明私有成员：最小堆容器、当前 Key/Value 缓存

- [x] Task 2: 实现最小堆比较器
  - [x] SubTask 2.1: 定义 `IteratorEntryCompare` 比较器结构体
  - [x] SubTask 2.2: 实现比较逻辑：Key 升序，Key 相同时优先级降序（优先级高的先出）
  - [x] SubTask 2.3: 使用 `std::priority_queue` 配合自定义比较器

- [x] Task 3: 实现构造函数与初始化
  - [x] SubTask 3.1: 接收 `std::vector<std::unique_ptr<SSTableReader::Iterator>>` 参数
  - [x] SubTask 3.2: 为每个迭代器分配优先级（索引越小优先级越高，代表越新）
  - [x] SubTask 3.3: 调用每个迭代器的 `SeekToFirst()`，将有效迭代器加入堆
  - [x] SubTask 3.4: 弹出堆顶作为当前元素

- [x] Task 4: 实现 Valid()、Key()、Value() 方法
  - [x] SubTask 4.1: `Valid()` 返回当前元素是否有效
  - [x] SubTask 4.2: `Key()` 返回当前 Key
  - [x] SubTask 4.3: `Value()` 返回当前 Value

- [x] Task 5: 实现 Next() 方法与重复 Key 处理
  - [x] SubTask 5.1: 弹出堆顶迭代器，调用其 `Next()` 方法
  - [x] SubTask 5.2: 如果迭代器仍有效，重新加入堆
  - [x] SubTask 5.3: 检查新堆顶 Key 是否与当前 Key 相同
  - [x] SubTask 5.4: 如果相同，跳过（保留优先级更高的版本），继续弹出直到 Key 不同或堆为空
  - [x] SubTask 5.5: 更新当前 Key/Value 缓存

- [x] Task 6: 编写单元测试
  - [x] SubTask 6.1: 测试用例 `MergeTwoSSTables`：合并 2 个有序 SSTable，验证输出有序
  - [x] SubTask 6.2: 测试用例 `MergeFourSSTables`：合并 4 个有序 SSTable，验证输出有序
  - [x] SubTask 6.3: 测试用例 `DuplicateKeyPriority`：同一 Key 在多个 SSTable 中存在不同值，验证保留最新版本
  - [x] SubTask 6.4: 测试用例 `TombstonePreserved`：验证 Tombstone 正确保留
  - [x] SubTask 6.5: 测试用例 `TombstoneOverwritten`：Tombstone 后有新值，验证输出新值
  - [x] SubTask 6.6: 测试用例 `EmptyInput`：空迭代器列表，验证 `Valid()` 返回 false

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 2
- Task 4 depends on Task 3
- Task 5 depends on Task 4
- Task 6 depends on Task 5
