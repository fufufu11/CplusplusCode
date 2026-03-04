# DistributedKV 测试记录文档

本文档专门用于记录项目的测试方法论、测试用例清单以及历史验收记录。

## 1. 测试体系概览 (GTest/CTest)

### 1.1 单元测试解决什么问题

单元测试（Unit Test）关注“一个很小的单元（函数/类/模块）在给定输入下是否输出符合预期”。它主要解决三类工程问题：
- **回归**：今天修好/写好的功能，明天改代码不会悄悄坏掉
- **定位**：出现 bug 时能快速把问题范围缩到某个模块或某条路径
- **重构保障**：你可以放心整理代码结构，只要测试还是绿的，就说明行为没变

### 1.2 本项目的测试入口

本项目的测试文件位于 `tests/` 目录：
- `tests/skiplist_test.cpp`：跳表核心逻辑测试（insert/search/remove）
- `tests/kv_store_test.cpp`：KVStore 整体集成测试（目录管理、WAL 检测、Put/Get 接口）
- `tests/wal_record_test.cpp`：WAL 记录编解码与 CRC32 校验测试
- `tests/sstable_builder_test.cpp`：SSTable 构造器测试（文件创建、数据写入、Footer 校验）
- `tests/sstable_reader_test.cpp`：SSTable 读取器测试（文件打开、迭代器、二分查找）
- `tests/flush_integration_test.cpp`：Flush 集成测试（MemTable 到 SSTable 的持久化流程）
- `tests/compaction_merger_test.cpp`：Compaction 多路归并排序测试（合并、重复 Key 处理、Tombstone 处理）

**运行方式**：
```powershell
# 方式 1: 使用 CTest (推荐)
ctest --test-dir build --output-on-failure

# 方式 2: 运行特定测试可执行文件
.\build\bin\skiplist_test.exe
```

### 1.3 写测试的 AAA 套路

把每个测试都按三段组织，读起来最清晰：
- **Arrange**：准备数据与环境（构造对象、准备输入）
- **Act**：执行动作（调用要测试的函数）
- **Assert**：断言结果（验证输出、状态、不变量）

### 1.4 让测试可复现

测试想要“稳定”，要尽量消除不确定性：
- **固定随机种子**：例如 `std::mt19937 rng(12345)`，确保 shuffle 结果固定
- **控制随机层数路径**：用 `prob=0.0f` 或 `prob=1.0f` 覆盖确定路径
- **避免依赖遍历顺序**：`unordered_map` 的遍历顺序不保证稳定，不要把“遍历顺序”作为测试依据

---

## 2. 测试用例清单与验收标准

### 2.1 SkipList 核心测试 (`tests/skiplist_test.cpp`)

覆盖跳表数据结构的核心 CRUD 操作：
- **空表行为**：查找/删除空表应返回未命中/false。
- **基础插入查询**：单条、批量、乱序插入后，查询结果应正确。
- **更新语义**：重复 Key 插入应覆盖 Value。
- **删除逻辑**：
    - 删除存在的 Key：后续不可查。
    - 删除不存在的 Key：返回 false，不破坏结构。
    - 删除边界 Key：最小/最大 Key 删除后结构仍完整。
- **极端层数**：在 `prob=1.0` (全满层) 和 `prob=0.0` (单层) 下逻辑均正确。

### 2.2 KVStore 集成测试 (`tests/kv_store_test.cpp`)

覆盖 KVStore 对外的集成行为（Task 1）：
- **目录管理**：自动创建 data 目录。
- **WAL 检测**：启动时能识别已存在的 `wal.log`。
- **接口打通**：`Put`/`Get`/`Delete` 能正确透传给底层的 SkipList。

### 2.3 WAL 编解码测试 (`tests/wal_record_test.cpp`)

覆盖持久化格式的底层逻辑 (Task 2)：
- **CRC32**：验证标准向量计算正确性。
- **Encode**：验证 `Put`/`Delete` 记录编码后的字节流长度、Header 字段及 Payload 内容。
- **Checksum**：验证编码中写入的 Checksum 与实际计算一致。

### 2.4 WAL 重放恢复测试 (`tests/kv_store_test.cpp`)

覆盖崩溃恢复场景 (Task 4) 以及崩溃模拟用例 (第 3 周 Task 6)：
- **NormalRecovery**：写入并销毁对象后，重建对象能恢复之前的数据（Put/Delete）。
- **TruncatedWAL**：WAL 尾部存在不完整记录（模拟断电半写），能自动忽略并恢复有效前缀。
- **CorruptedWAL**：WAL 中间数据损坏（Checksum 不匹配），能识别并停止重放（Fail-Stop）。
- **BulkRecovery1000**：写入 1000 条 Put，模拟重启后全部可读。
- **MixedPutDelRecovery**：Put/Delete/覆盖更新混合写入，模拟重启后最终状态正确。
- **TruncateMidRecord**：将 WAL 截断到某条记录中间，重启仅恢复完整前缀且不崩溃。
- **CorruptMiddleRecordStopsAtPrefix**：中间记录损坏时，重启仅恢复损坏记录之前的前缀并停止。

### 2.5 SSTable 构造器测试 (`tests/sstable_builder_test.cpp`)

覆盖 SSTable 文件构建的核心逻辑 (第 4 周 Task 2)：
- **文件创建**：验证 SSTable 文件能正确创建，至少包含 48 字节 Footer。
- **数据写入**：验证 `Add()` 方法正常工作，文件大小随数据增长。
- **Footer 校验**：验证 Footer 末尾 Magic Number 正确。
- **RAII 行为**：验证析构函数自动调用 `Finish()`，即使忘记显式调用。
- **多 Block 场景**：验证大量数据触发多个 Block 写入。
- **状态管理**：验证 `FileSize()` 和 `Finished()` 返回值正确。
- **异常处理**：验证重复调用 `Finish()` 抛出异常。

### 2.6 SSTable 读取器测试 (`tests/sstable_reader_test.cpp`)

覆盖 SSTable 文件读取的核心逻辑 (第 4 周 Task 3)：
- **文件打开**：验证能正确打开有效的 SSTable 文件，抛出异常处理无效文件。
- **单 Key 查询**：验证 `Get()` 方法能正确返回存在的 Key，返回 nullopt 处理不存在的 Key。
- **多 Key 查询**：验证批量 Key 查询的正确性。
- **迭代器基础**：验证 `SeekToFirst()`、`Next()`、`Valid()` 的正确行为。
- **迭代器 Seek**：验证 `Seek(target)` 能定位到第一个 >= target 的 Key。
- **迭代器 Seek 边界**：验证 Seek 不存在的 Key 时返回下一个 Key，Seek 超出范围时返回无效状态。
- **大数据集**：验证 1000 条数据的读写正确性。
- **多 Block 迭代**：验证迭代器能正确跨 Block 遍历，顺序正确。
- **空 Value 处理**：验证空字符串 Value 的正确读写。

### 2.7 Flush 集成测试 (`tests/flush_integration_test.cpp`)

覆盖 MemTable 到 SSTable 的 Flush 流程 (第 4 周 Task 4-5)：
- **手动 Flush**：验证调用 `Flush()` 后生成 SSTable 文件。
- **Flush 后读取**：验证 Flush 后能从 SSTable 读取数据。
- **自动 Flush 触发**：验证 MemTable 达到阈值时自动触发 Flush。
- **多次 Flush**：验证多次 Flush 生成多个 SSTable 文件，数据都能正确读取。
- **MemTable 优先级**：验证查询时 MemTable 数据优先于 SSTable。
- **重启恢复**：验证 Flush 后重启能正确加载 SSTable 数据。
- **WAL 清理**：验证 Flush 后 WAL 被清空。
- **大数据集往返**：验证大批量数据写入、Flush、重启后数据完整性。
- **Flush 后删除**：验证 Flush 后的删除操作（当前阶段仅对 MemTable 有效）。
- **Flush 后更新**：验证 Flush 后对已存在 Key 的更新操作。

---

## 3. 历史测试记录

### 第 1-2 周：SkipList 与基础架构

- **2026-01-31**：SkipList 核心功能验收通过
    - 结果：`14/14 tests passed`
    - 范围：覆盖 Insert/Search/Remove 及边界条件。

- **2026-01-31**：KVStore 骨架验收通过 (Task 1)
    - 结果：`19/19 tests passed` (含 SkipList 14 个)
    - 新增用例：`CreatesDataDirectory`, `DetectsExistingWAL`, `BasicPutGet` 等。

### 第 3 周：WAL 预写日志

- **2026-02-04**：WAL 编解码与校验验收通过 (Task 2)
    - 结果：`22/22 tests passed` (全量)
    - 新增用例：`CRC32Test`, `WALRecordTest` (EncodePut/EncodeDelete)。

- **2026-02-05**：WAL 写入与持久化验收通过 (Task 3)
    - 结果：`23/23 tests passed`
    - 新增用例：`KVStoreTest.WALPersistenceCheck`。
    - 验证点：`Put`/`Delete` 操作后，WAL 文件被创建且包含预期数据（强持久化生效）。

- **2026-02-07**：WAL 读取与重放验收通过 (Task 4)
    - 结果：`26/26 tests passed`
    - 新增用例：`WALReplayTest` (`NormalRecovery`, `TruncatedWAL`, `CorruptedWAL`)。
    - 验证点：成功实现 Read -> Verify -> Apply 闭环；对崩溃截断和数据损坏有正确的容错处理。

- **2026-02-08**：崩溃模拟用例补齐验收通过 (Task 6)
    - 结果：`30/30 tests passed`
    - 新增用例：`WALReplayTest` (`BulkRecovery1000`, `MixedPutDelRecovery`, `TruncateMidRecord`, `CorruptMiddleRecordStopsAtPrefix`)。
    - 验证点：覆盖大批量重启恢复、Put/Delete 混合恢复、记录中途截断恢复前缀、以及中间记录损坏 fail-stop 行为。

### 第 4 周：SSTable 文件格式与 Flush 流程

- **2026-02-15**：SSTable 构造器验收通过 (Task 2)
    - 结果：`38/38 tests passed` (全量)
    - 新增用例：`SSTableBuilderTest` (8 个)
        - `CreatesFileWithFooter`：验证空 SSTable 文件至少包含 48 字节 Footer
        - `WritesSmallData`：验证少量 KV 对写入后文件增长
        - `FooterMagicNumberCorrect`：验证 Footer 末尾 Magic Number 正确
        - `AutoFinishOnDestruction`：验证 RAII 行为（析构时自动 Finish）
        - `WritesMultipleBlocks`：验证大量数据触发多个 Block 写入
        - `FileSizeAccurate`：验证 `FileSize()` 返回值与实际文件大小一致
        - `FinishedStateCorrect`：验证 `Finished()` 状态转换正确
        - `DoubleFinishThrows`：验证重复调用 `Finish()` 抛出异常
    - 验证点：SSTableBuilder 能正确创建文件、写入数据、生成 Footer，RAII 行为正确。

- **2026-02-23**：SSTable 读取器验收通过 (Task 3)
    - 结果：`52/52 tests passed` (全量)
    - 新增用例：`SSTableReaderTest` (14 个)
        - `OpenValidFile`：验证能正确打开有效的 SSTable 文件
        - `OpenNonExistentFile`：验证打开不存在文件时抛出异常
        - `GetSingleKey`：验证单 Key 查询
        - `GetNonExistentKey`：验证查询不存在的 Key 返回 nullopt
        - `GetMultipleKeys`：验证多 Key 批量查询
        - `IteratorSeekToFirst`：验证迭代器定位到第一个元素
        - `IteratorSequentialScan`：验证迭代器顺序遍历
        - `IteratorSeekMiddle`：验证迭代器 Seek 到中间位置
        - `IteratorSeekNonExistent`：验证 Seek 不存在的 Key 返回下一个
        - `IteratorSeekPastEnd`：验证 Seek 超出范围返回无效状态
        - `LargeDataset`：验证 1000 条数据读写
        - `MultipleBlocksIterator`：验证跨 Block 迭代
        - `IndexEntriesLoaded`：验证 Index Block 正确加载
        - `EmptyValue`：验证空 Value 处理
    - 验证点：SSTableReader 能正确读取文件、解析 Footer 和 Index Block、支持二分查找和迭代器遍历。

- **2026-02-23**：Flush 集成与端到端验收通过 (Task 4-5)
    - 结果：`62/62 tests passed` (全量)
    - 新增用例：`FlushIntegrationTest` (10 个)
        - `ManualFlushCreatesSSTable`：验证手动 Flush 生成 SSTable 文件
        - `ReadFromFlushedSSTable`：验证从 Flush 后的 SSTable 读取数据
        - `AutoFlushTriggered`：验证 MemTable 达到阈值自动 Flush
        - `MultipleFlushes`：验证多次 Flush 生成多个 SSTable
        - `MemTablePriorityOverSSTable`：验证 MemTable 查询优先级
        - `RecoveryAfterFlush`：验证 Flush 后重启恢复
        - `WALClearedAfterFlush`：验证 Flush 后 WAL 被清空
        - `LargeDatasetRoundTrip`：验证大数据集写入、Flush、重启后完整性
        - `DeleteAfterFlush`：验证 Flush 后的删除操作
        - `UpdateAfterFlush`：验证 Flush 后的更新操作
    - 验证点：实现了从 MemTable 到 SSTable 的完整 Flush 流程，支持自动/手动触发，WAL 清理，重启恢复。

    **测试失败记录与修复过程**：

    **问题 1：SkipList 不支持移动语义和 clear() 方法**
    - **现象**：编译错误，`SkipList` 包含 `unique_ptr` 的 vector，无法使用拷贝赋值
    - **原因**：`KVStore::Flush()` 中尝试 `memtable_ = SkipList<int, std::string>(6)` 重置 MemTable
    - **修复**：为 `SkipList` 添加移动构造/移动赋值运算符和 `clear()` 方法，改用 `memtable_.clear()` 重置

    **问题 2：字符串 Key 排序不一致导致 SSTable 查找失败**
    - **现象**：`FlushIntegrationTest.LargeDatasetRoundTrip` 测试失败，部分 Key 查询返回 nullopt
    - **原因**：整数转字符串后，字符串比较顺序与整数不同（如 "10" < "2"），导致 SSTable 二分查找定位错误
    - **修复**：在 `KVStore` 中使用 `FormatKey()` 方法将整数 Key 格式化为 11 位定长字符串（`%011d`），确保字符串排序与整数排序一致

    **问题 3：WAL 和 SSTable 使用不同的 Key 格式**
    - **现象**：WAL replay 后数据无法正确恢复到 MemTable
    - **原因**：WAL 写入使用 `std::to_string(key)`，但 SSTable 查询使用格式化的 Key
    - **修复**：统一 WAL 写入也使用 `FormatKey()` 方法，确保 Key 格式一致

    **问题 4：测试用例直接操作 WAL 文件时 Key 格式不匹配**
    - **现象**：`WALReplayTest.TruncateMidRecord` 和 `CorruptMiddleRecordStopsAtPrefix` 测试失败
    - **原因**：测试用例手动构造 WAL 记录时使用 `std::to_string(i)` 而非格式化 Key
    - **修复**：更新测试用例使用 `std::snprintf(buf, sizeof(buf), "%011d", i)` 构造格式化 Key

    **问题 5：SSTable 二分查找边界条件处理不当**
    - **现象**：查询大于最后一个 Block 的 last_key 的 Key 时错误返回数据
    - **原因**：二分查找使用 `upper_bound`，当 key > last_key 时仍返回最后一个 Block
    - **修复**：在 `SSTableReader::Get()` 中增加边界检查，当 key > 最后一个 Block 的 last_key 时直接返回 nullopt

### 第 5 周：查询路径与 Tombstone 机制

- **2026-02-24**：ValueType 枚举与 Tombstone 机制验收通过
    - 结果：`67/67 tests passed` (全量)
    - 新增用例：`KVStoreTest` (5 个 Tombstone 相关)
        - `StoreSpecialStringTombstone`：验证用户可以存储 `"__tombstone__"` 字符串并正确读回
        - `DeleteThenGetReturnsNullopt`：验证删除后查询返回 nullopt
        - `DeleteThenRewriteReturnsNewValue`：验证删除后重新写入返回新值
        - `TombstonePersistsAfterRecovery`：验证重启后删除语义正确
        - `TombstonePersistsAfterFlush`：验证 Flush 后删除语义正确
    - 验证点：
        - ValueType 枚举（NORMAL/TOMBSTONE）正确区分普通值和删除标记
        - 用户可存储任意字符串（包括 `"__tombstone__"`），不会与删除标记冲突
        - WAL 和 SSTable 格式扩展（增加 1 字节 ValueType 字段）
        - 删除操作写入 Tombstone 而非物理删除
        - 查询时遇到 Tombstone 返回 nullopt 并停止查找

    **设计变更记录**：

    **问题：使用特殊字符串作为 Tombstone 标记存在冲突**
    - **现象**：如果用户执行 `put(key, "__tombstone__")`，该值会被误认为删除标记
    - **原因**：原设计使用字符串常量 `"__tombstone__"` 作为删除标记
    - **修复**：引入 `ValueType` 枚举类型，区分普通值和删除标记
        ```cpp
        enum class ValueType : uint8_t {
            NORMAL = 0,      // 普通值
            TOMBSTONE = 1    // 删除标记
        };
        
        struct Value {
            std::string data;
            ValueType type = ValueType::NORMAL;
            bool is_tombstone() const { return type == ValueType::TOMBSTONE; }
        };
        ```
    - **影响**：BREAKING 变更，旧格式的 WAL 和 SSTable 文件不兼容

    **修改的文件**：
    - `include/value_type.h` (新增)
    - `include/wal_record.h` (WAL 格式扩展)
    - `include/sstable_builder.h` (SSTable 格式扩展)
    - `include/sstable_reader.h` (返回 Value 结构体)
    - `include/kv_store.h` (Tombstone 处理逻辑)

- **2026-02-24**：多 SSTable 查询优化验收通过 (Task 4)
    - 结果：`70/70 tests passed` (全量)
    - 新增用例：`FlushIntegrationTest` (3 个)
        - `MultiSSTableVersionPriority`：验证同一 Key 在多个 SSTable 中存在不同值时，返回最新 SSTable 的值
        - `MultiSSTableWithTombstone`：验证多个 SSTable 中存在 Tombstone 时删除语义正确
        - `MultiSSTableRecovery`：验证重启后多 SSTable 查询正确
    - 验证点：
        - 查询顺序正确：MemTable → SSTable（从新到旧，使用 `rbegin()/rend()` 反向遍历）
        - 多 SSTable 版本可见性：同一 Key 在多个 SSTable 中存在时，返回最新版本
        - Tombstone 跨 SSTable 生效：即使旧 SSTable 中有数据，新 SSTable 中的 Tombstone 仍能正确屏蔽
        - 重启后多 SSTable 查询一致性：加载顺序与查询顺序匹配

    **第 5 周完成总结**：
    - 任务一：理解读路径分层架构 ✅
    - 任务二：完善 Get 查询链路 ✅
    - 任务三：Tombstone 机制 ✅
    - 任务四：多 SSTable 查询优化 ✅
    - 任务五：单元测试与验证 ✅

### 第 6 周：Compaction 合并

- **2026-02-28**：Compaction 触发机制验收通过 (Task 2)
    - 结果：`22/22 tests passed` (全量)
    - 新增用例：`CompactionTriggerTest` (4 个)
        - `GetL0FileCountReturnsCorrectCount`：验证 L0 文件计数正确
        - `ShouldCompactReturnsFalseBelowThreshold`：验证低于阈值时不触发
        - `ShouldCompactReturnsTrueAtThreshold`：验证达到阈值时触发
        - `CustomThresholdConfigWorks`：验证自定义阈值配置生效
    - 验证点：
        - `CompactionConfig` 结构体定义正确，默认阈值 4
        - `GetL0FileCount()` 正确统计 L0 层文件数量
        - `ShouldCompact()` 在 L0 文件数量 ≥ 阈值时返回 true
        - `Flush()` 完成后调用 `ShouldCompact()` 检查

- **2026-02-28**：多路归并排序验收通过 (Task 3-4)
    - 结果：`31/31 tests passed` (全量，含新增 9 个 CompactionMerger 测试)
    - 新增用例：`CompactionMergerTest` (9 个)
        - `MergeTwoSSTables`：验证合并 2 个有序 SSTable 输出有序
        - `MergeFourSSTables`：验证合并 4 个有序 SSTable 输出有序
        - `DuplicateKeyPriority`：验证同一 Key 在多个 SSTable 中存在不同值时，保留最新版本
        - `TombstonePreserved`：验证 Tombstone 在无新值覆盖时正确保留
        - `TombstoneOverwritten`：验证 Tombstone 后有新值时输出新值
        - `EmptyInput`：验证空迭代器列表时 `Valid()` 返回 false
        - `SingleSSTable`：验证单个 SSTable 的正确处理
        - `NullIteratorsSkipped`：验证空迭代器被正确跳过
        - `AllSameKeys`：验证所有 Key 相同时只保留最高优先级版本
    - 验证点：
        - `CompactionMerger` 类使用最小堆实现多路归并
        - 迭代器优先级正确：索引越小优先级越高（代表越新的 SSTable）
        - 重复 Key 正确处理：保留优先级更高的版本
        - Tombstone 正确保留或被新值覆盖
        - 流式处理，内存占用与迭代器数量成正比

    **实现细节**：

    **核心类设计**：
    - `IteratorEntry`：包装迭代器，包含优先级和当前 KV 缓存
    - `IteratorEntryCompare`：自定义比较器，Key 升序 + 同 Key 时优先级高的先出
    - `CompactionMerger`：多路归并器主类，提供 `Valid()`、`Key()`、`GetValue()`、`Next()` 接口

    **测试失败记录与修复过程**：

    **问题 1：迭代器未调用 SeekToFirst()**
    - **现象**：测试失败，`result.size()` 返回 0
    - **原因**：构造函数中未调用迭代器的 `SeekToFirst()` 方法
    - **修复**：在构造函数中添加 `entry->iter->SeekToFirst()` 调用

    **问题 2：SSTableReader 生命周期问题**
    - **现象**：测试崩溃（访问已释放的内存）
    - **原因**：`CreateIterator()` 函数中创建的 `SSTableReader` 在函数返回后被销毁，导致迭代器无效
    - **修复**：在测试夹具中添加 `readers_` 成员持有 `SSTableReader` 的所有权，确保迭代器生命周期内有效

- **2026-02-28**：L0 → L1 Compaction 流程验收通过 (Task 5-7)
    - 结果：`84/84 tests passed` (全量)
    - 新增/修改用例：`CompactionTriggerTest` (更新 2 个)
        - `ShouldCompactReturnsTrueAtThreshold`：验证达到阈值时触发 Compaction，L0 文件合并为 L1，数据完整可查
        - `CustomThresholdConfigWorks`：验证自定义阈值触发 Compaction，合并后数据正确
    - 验证点：
        - `DoCompaction()` 方法正确实现 L0 → L1 合并流程
        - L0 文件选择逻辑正确收集所有 L0 文件
        - `CompactionMerger` 正确用于多路归并
        - `SSTableBuilder` 正确生成 L1 文件
        - 临时文件机制保证原子性（`L1_temp_*.sst` → `L1_*.sst`）
        - 旧 L0 文件在 Compaction 成功后被删除
        - `sstable_readers_` 列表正确更新
        - Get 查询在 Compaction 后返回正确结果
        - 重复 Key 只保留最新版本
        - Tombstone 正确保留在 L1 文件中

    **实现细节**：

    **DoCompaction() 核心流程**：
    1. 收集所有 L0 文件路径
    2. 为每个 L0 文件创建 `SSTableReader` 和迭代器
    3. 反转迭代器顺序（索引越小优先级越高 = 越新）
    4. 使用 `CompactionMerger` 进行多路归并
    5. 使用 `SSTableBuilder` 生成临时 L1 文件
    6. 原子重命名为正式 L1 文件
    7. 更新 `sstable_readers_` 列表
    8. 释放 Reader 资源后删除旧 L0 文件

    **测试失败记录与修复过程**：

    **问题 1：GetL0FileCount() 返回所有 SSTable 数量**
    - **现象**：Compaction 后 `GetL0FileCount()` 返回 1 而非 0
    - **原因**：原实现返回 `sstable_readers_.size()`，包含 L1 文件
    - **修复**：改为遍历目录统计以 `L0_` 开头的 `.sst` 文件数量

    **问题 2：SSTableReader 生命周期导致崩溃**
    - **现象**：Compaction 过程中程序崩溃
    - **原因**：`DoCompaction()` 中创建的 `SSTableReader` 在迭代器使用期间被销毁
    - **修复**：添加 `readers` 向量持有 `SSTableReader` 所有权，确保迭代器生命周期内有效

    **问题 3：文件删除失败（文件被占用）**
    - **现象**：删除 L0 文件时抛出异常 "另一个程序正在使用此文件"
    - **原因**：`readers` 向量中的 `SSTableReader` 仍持有文件句柄
    - **修复**：在删除文件前调用 `readers.clear()` 释放所有 Reader

    **第 6 周完成总结**：
    - 任务一：理解 Compaction 核心概念 ✅
    - 任务二：设计 Compaction 触发机制 ✅
    - 任务三：实现多路归并排序 ✅
    - 任务四：处理重复 Key 和 Tombstone ✅
    - 任务五：实现 L0 → L1 Compaction 流程 ✅
    - 任务六：文件管理与原子性保证 ✅
    - 任务七：单元测试与验证 ✅

### 第 7 周：Raft 状态机框架

- **2026-03-01**：Raft 状态机框架验收通过
    - 结果：`127/127 tests passed` (全量)
    - 新增用例：`RaftNodeTest` (41 个) + `StateToStringTest` (2 个)
        - `InitialStateIsFollower`：验证节点初始状态为 Follower
        - `InitialTermIsZero`：验证初始任期为 0
        - `InitialVotedForIsEmpty`：验证初始未投票
        - `InitialVotedCountIsZero`：验证初始投票计数为 0
        - `BecomeFollowerChangesState`：验证状态转换为 Follower
        - `BecomeFollowerUpdatesTerm`：验证转换 Follower 时更新任期
        - `BecomeFollowerResetsVotedCount`：验证转换 Follower 时重置投票计数
        - `BecomeFollowerClearsVotedForOnHigherTerm`：验证更高任期时清除投票
        - `BecomeFollowerKeepsVotedForOnSameTerm`：验证相同任期时保留投票
        - `BecomeCandidateChangesState`：验证状态转换为 Candidate
        - `BecomeCandidateIncrementsTerm`：验证转换 Candidate 时增加任期
        - `BecomeCandidateVotesForSelf`：验证 Candidate 投票给自己
        - `BecomeCandidateSetsVotedCountToOne`：验证投票计数设为 1
        - `BecomeLeaderChangesState`：验证状态转换为 Leader
        - `BecomeLeaderResetsVotedCount`：验证转换 Leader 时重置投票计数
        - `ElectionTimeoutWithinRange`：验证选举超时在指定范围内
        - `ResetElectionTimeoutProducesDifferentValues`：验证超时随机化
        - `CheckElectionTimeoutReturnsFalseImmediately`：验证立即检查不超时
        - `CheckElectionTimeoutReturnsTrueAfterTimeout`：验证超时后检测正确
        - `CheckElectionTimeoutAlwaysFalseForLeader`：验证 Leader 不检测超时
        - `UpdateHeartbeatResetsTimeout`：验证心跳更新重置超时
        - `SetCurrentTerm`：验证设置任期
        - `SetVotedFor`：验证设置投票对象
        - `SetVotedCount`：验证设置投票计数
        - `IncrementVotedCount`：验证增加投票计数
        - `CanVoteForWhenNotVoted`：验证未投票时可投票
        - `CanVoteForSameCandidate`：验证可投给同一候选人
        - `CannotVoteForDifferentCandidate`：验证不可投给不同候选人
        - `CannotVoteForLowerTerm`：验证拒绝低任期投票请求
        - `CanVoteForHigherTerm`：验证接受高任期投票请求
        - `VoteForSetsVotedFor`：验证投票设置正确
        - `VoteForUpdatesTermIfHigher`：验证高任期更新
        - `VoteForKeepsTermIfLower`：验证低任期不更新
        - `NodeIdIsCorrect`：验证节点 ID 正确
        - `CustomElectionTimeoutRange`：验证自定义超时范围
        - `LastHeartbeatUpdatedOnReset`：验证重置时更新心跳时间
        - `LastHeartbeatUpdatedOnBecomeFollower`：验证转换时更新心跳时间
        - `StateTransitionSequence`：验证完整状态转换序列
        - `MultipleCandidatesCanCoexist`：验证多候选人共存
        - `FollowerRespondsToHigherTerm`：验证 Follower 响应更高任期
        - `ElectionTimeoutAfterStateChange`：验证状态变更后超时检测
        - `StateToStringTest.ReturnsCorrectStrings`：验证状态转字符串
        - `StateToStringTest.ReturnsUnknownForInvalidValue`：验证无效状态处理
    - 验证点：
        - `RaftNode` 类定义完整，包含 State 枚举和核心数据结构
        - 状态转换方法 `become_leader()`, `become_follower()`, `become_candidate()` 正确实现
        - 选举超时检测 `check_election_timeout()` 和随机化逻辑正常工作
        - `RequestVote` RPC 结构体定义正确
        - 投票逻辑 `can_vote_for()`, `vote_for()` 正确实现

    **第 7 周完成总结**：
    - Step 1：`RaftNode` 类定义 ✅
    - Step 2：状态转换方法实现 ✅
    - Step 3：选举超时检测和随机化逻辑 ✅
    - Step 4：`RequestVote` RPC 结构体定义 ✅
    - Step 5：单元测试与验证 ✅

- **2026-03-03**：第七周性能指标验收通过
    - 结果：`45/45 tests passed` (含新增 2 个性能测试)
    - 新增用例：`RaftPerformanceTest` (2 个)
        - `StateTransitionLatencyUnder1ms`：验证状态转换延迟 < 1ms
        - `ElectionTimeoutPrecisionErrorUnder10Percent`：验证选举超时精度误差 < 10%
    - 验证点：
        - **状态转换延迟**：`become_leader()`, `become_follower()`, `become_candidate()` 三种状态转换方法的执行延迟均在纳秒级别，远小于 1ms 目标 ✅
        - **选举超时精度误差**：100 次测试中，最大误差和平均误差均小于 10% 目标 ✅

    **性能测试详情**：

    | 性能指标 | 目标值 | 实测结果 | 状态 |
    |---------|--------|---------|------|
    | 状态转换延迟 | < 1ms | 纳秒级（< 1μs） | ✅ 通过 |
    | 选举超时精度误差 | < 10% | 最大误差 < 10%，平均误差 < 10% | ✅ 通过 |

    **测试方法说明**：
    - **状态转换延迟测试**：使用 `std::chrono::high_resolution_clock` 测量 10000 次状态转换的执行时间，取最大值验证
    - **选举超时精度测试**：使用固定 50ms 超时，测量 100 次实际超时触发时刻与预期时刻的偏差百分比
