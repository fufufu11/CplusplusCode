# Checklist

- [x] `CompactionConfig` 结构体定义正确，包含 `l0_trigger_count` 成员且默认值为 4
- [x] `KVStore` 构造函数支持可选的 `CompactionConfig` 参数
- [x] `GetL0FileCount()` 能正确统计 L0 层文件数量
- [x] `ShouldCompact()` 在 L0 文件数量 < 阈值时返回 `false`
- [x] `ShouldCompact()` 在 L0 文件数量 ≥ 阈值时返回 `true`
- [x] `Flush()` 完成后调用 `ShouldCompact()` 检查
- [x] 自定义阈值配置能正确生效
- [x] 所有单元测试通过
