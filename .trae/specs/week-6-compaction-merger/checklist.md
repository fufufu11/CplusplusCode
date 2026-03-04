# Checklist

- [x] `CompactionMerger` 类定义正确，包含迭代器接口 `Valid()`、`Key()`、`Value()`、`Next()`
- [x] 最小堆比较器正确实现：Key 升序，Key 相同时优先级高的先出
- [x] 构造函数正确初始化堆，所有有效迭代器加入堆
- [x] `Valid()` 在有数据时返回 `true`，无数据时返回 `false`
- [x] `Key()` 和 `Value()` 正确返回当前元素
- [x] `Next()` 正确移动到下一个元素
- [x] 重复 Key 正确处理：保留优先级更高（更新）的版本
- [x] Tombstone 正确保留（删除后无新值场景）
- [x] Tombstone 被新值覆盖（删除后有新值场景）
- [x] 合并 2 个 SSTable 输出有序
- [x] 合并 4 个 SSTable 输出有序
- [x] 空输入时 `Valid()` 返回 `false`
- [x] 所有单元测试通过
