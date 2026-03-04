# Tombstone 特殊值类型 Spec

## Why
当前文档建议使用字符串常量 `"__tombstone__"` 作为删除标记，但如果用户真实存储这个值，会产生冲突，导致数据丢失。需要引入 `ValueType` 枚举来区分普通值和删除标记。

## What Changes
- 新增 `ValueType` 枚举类型，区分 `NORMAL` 和 `TOMBSTONE`
- 修改 `Value` 结构体，包含数据和类型信息
- 修改 WAL 记录格式，支持存储 ValueType
- 修改 SSTable Entry 格式，支持存储 ValueType
- 修改 `KVStore::get()` 正确处理 Tombstone
- 修改 `KVStore::del()` 写入 Tombstone 而非物理删除
- **BREAKING**: WAL 和 SSTable 文件格式变更，旧文件不兼容

## Impact
- Affected specs: Week 5 查询路径任务
- Affected code:
  - `include/value_type.h` (新增)
  - `include/wal_record.h` (修改)
  - `include/sstable_builder.h` (修改)
  - `include/sstable_reader.h` (修改)
  - `include/kv_store.h` (修改)
  - `tests/` (新增测试)

## ADDED Requirements

### Requirement: ValueType 枚举定义
系统应提供 `ValueType` 枚举类型，用于区分普通值和删除标记。

```cpp
enum class ValueType : uint8_t {
    NORMAL = 0,
    TOMBSTONE = 1
};
```

### Requirement: Value 结构体
系统应提供 `Value` 结构体，封装数据和类型信息。

```cpp
struct Value {
    std::string data;
    ValueType type = ValueType::NORMAL;
    
    bool is_tombstone() const { return type == ValueType::TOMBSTONE; }
    
    static Value normal(const std::string& d) { return Value{d, ValueType::NORMAL}; }
    static Value tombstone() { return Value{"", ValueType::TOMBSTONE}; }
};
```

### Requirement: WAL 记录格式扩展
WAL 记录应包含 ValueType 字段，编码格式变更为：
- `Checksum (4B) | KeyLen (4B) | ValueLen (4B) | Type (1B) | ValueType (1B) | Key | Value`

#### Scenario: 写入普通值
- **WHEN** 用户执行 `put(key, "hello")`
- **THEN** WAL 记录中 ValueType = NORMAL，Value = "hello"

#### Scenario: 写入删除标记
- **WHEN** 用户执行 `del(key)`
- **THEN** WAL 记录中 ValueType = TOMBSTONE，Value = ""

### Requirement: SSTable Entry 格式扩展
SSTable Entry 应包含 ValueType 字段，编码格式变更为：
- `KeyLen (4B) | ValueLen (4B) | ValueType (1B) | Key | Value`

### Requirement: KVStore::get() Tombstone 处理
查询时遇到 Tombstone 应返回 `std::nullopt` 并停止继续查找。

#### Scenario: 查询被删除的 Key
- **GIVEN** Key A 在 L0_001.sst 中存在值为 "v1"
- **AND** 用户执行 `del(A)` 写入 Tombstone 到 MemTable
- **WHEN** 用户执行 `get(A)`
- **THEN** 返回 `std::nullopt`（不继续查 L0_001.sst）

#### Scenario: 删除后重新写入
- **GIVEN** Key A 在 L0_001.sst 中存在值为 "v1"
- **AND** 用户执行 `del(A)` 写入 Tombstone 到 L0_002.sst
- **AND** 用户执行 `put(A, "v2")` 写入 MemTable
- **WHEN** 用户执行 `get(A)`
- **THEN** 返回 "v2"（MemTable 优先级最高）

### Requirement: KVStore::del() Tombstone 写入
删除操作应写入 Tombstone 而非物理删除。

#### Scenario: 删除操作
- **WHEN** 用户执行 `del(key)`
- **THEN** 写入 WAL 记录 (Type=kDelete, ValueType=TOMBSTONE)
- **AND** 写入 MemTable (key, Value::tombstone())

### Requirement: 用户可存储任意字符串
用户应能存储任意字符串值，包括 `"__tombstone__"`。

#### Scenario: 存储特殊字符串
- **WHEN** 用户执行 `put(key, "__tombstone__")`
- **AND** 用户执行 `get(key)`
- **THEN** 返回 `"__tombstone__"`（而非 `std::nullopt`）

## MODIFIED Requirements

### Requirement: WAL 重放逻辑
重放 WAL 时应根据 ValueType 正确处理：
- ValueType::NORMAL → 执行 `memtable_.insert(key, value)`
- ValueType::TOMBSTONE → 执行 `memtable_.insert(key, Value::tombstone())`

### Requirement: SSTable 迭代器
迭代器应返回完整的 Value 结构体（包含 data 和 type），而非仅字符串。

### Requirement: 文档同步更新
实现完成后，需更新以下文档章节，保持内容同步：
- `docs/DistributedKV_Guide/chapters/06-读路径分层架构.md`：更新 Tombstone 机制章节，说明使用 ValueType 枚举
- `docs/DistributedKV_Guide/chapters/13-项目计划周级细化.md`：更新第 5 周任务描述，反映 ValueType 方案

## REMOVED Requirements
无。
