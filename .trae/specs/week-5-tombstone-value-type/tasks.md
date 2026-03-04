# Tasks

- [x] Task 1: 定义 ValueType 枚举和 Value 结构体
    - [x] SubTask 1.1: 创建 `include/value_type.h`，定义 `ValueType` 枚举
    - [x] SubTask 1.2: 定义 `Value` 结构体，包含 `data` 和 `type` 成员
    - [x] SubTask 1.3: 添加 `is_tombstone()` 方法和静态工厂方法

- [x] Task 2: 修改 WAL 记录格式支持 ValueType
    - [x] SubTask 2.1: 修改 `LogRecord` 结构体，Value 字段类型改为 `Value` 结构体
    - [x] SubTask 2.2: 修改 `encode_log_record()` 函数，编码 ValueType 字段
    - [x] SubTask 2.3: 修改 `KVStore::replay_wal()` 解码 ValueType 字段
    - [x] SubTask 2.4: 编写单元测试验证 WAL 编解码

- [x] Task 3: 修改 SSTable 格式支持 ValueType
    - [x] SubTask 3.1: 修改 `SSTableBuilder::Add()` 接受 `Value` 结构体，编码 ValueType
    - [x] SubTask 3.2: 修改 `SSTableReader::Get()` 返回 `std::optional<Value>`
    - [x] SubTask 3.3: 修改 `SSTableReader::Iterator` 返回 `Value` 结构体
    - [x] SubTask 3.4: 编写单元测试验证 SSTable 读写

- [x] Task 4: 修改 KVStore 核心逻辑
    - [x] SubTask 4.1: 修改 `KVStore::put()` 接受 `std::string`，内部封装为 `Value::normal()`
    - [x] SubTask 4.2: 修改 `KVStore::get()` 正确处理 Tombstone（遇到返回 nullopt）
    - [x] SubTask 4.3: 修改 `KVStore::del()` 写入 Tombstone 而非物理删除
    - [x] SubTask 4.4: 修改 `KVStore::Flush()` 正确序列化 Value 结构体

- [x] Task 5: 集成测试与验证
    - [x] SubTask 5.1: 测试存储特殊字符串 `"__tombstone__"` 正常返回
    - [x] SubTask 5.2: 测试删除后查询返回 nullopt
    - [x] SubTask 5.3: 测试删除后重新写入返回新值
    - [x] SubTask 5.4: 测试重启后 Tombstone 语义正确

- [x] Task 6: 文档同步更新
    - [x] SubTask 6.1: 更新 `docs/DistributedKV_Guide/chapters/06-读路径分层架构.md`，修改 Tombstone 机制章节，说明使用 ValueType 枚举方案
    - [x] SubTask 6.2: 更新 `docs/DistributedKV_Guide/chapters/13-项目计划周级细化.md`，修改第 5 周任务描述，反映 ValueType 方案

# Task Dependencies
- Task 2 depends on Task 1 (WAL 需要 Value 类型)
- Task 3 depends on Task 1 (SSTable 需要 Value 类型)
- Task 4 depends on Task 2, Task 3 (KVStore 依赖 WAL 和 SSTable)
- Task 5 depends on Task 4 (测试依赖完整实现)
- Task 6 depends on Task 5 (文档更新在实现完成后)
