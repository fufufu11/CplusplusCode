# Tasks

- [x] Task 1: 理解读路径分层架构（理论学习）
    - [x] SubTask 1.1: 学习 LSM-Tree 读路径原理
    - [x] SubTask 1.2: 理解多 SSTable 场景查询顺序
    - [x] SubTask 1.3: 理解版本可见性规则

- [x] Task 2: 完善 Get 查询链路
    - [x] SubTask 2.1: 验证当前 `KVStore::get()` 实现正确性
    - [x] SubTask 2.2: 补充多 SSTable 查询顺序测试
    - [x] SubTask 2.3: 更新项目计划文档，标记任务一完成

- [x] Task 3: 多 SSTable 查询优化测试
    - [x] SubTask 3.1: 添加 `MultiSSTableVersionPriority` 测试
    - [x] SubTask 3.2: 添加 `MultiSSTableWithTombstone` 测试
    - [x] SubTask 3.3: 添加 `MultiSSTableRecovery` 测试

# Task Dependencies
- Task 2 depends on Task 1 (理解理论后再验证实现)
- Task 3 depends on Task 2 (测试依赖查询链路正确)
