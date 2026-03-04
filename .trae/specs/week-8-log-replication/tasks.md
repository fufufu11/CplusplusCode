# Tasks

## Day 1-2 任务（已完成）

### 代码实现部分

* [x] Task 1: 创建日志条目结构

  * [x] SubTask 1.1: 创建 src/raft/log_entry.h 文件

  * [x] SubTask 1.2: 定义 CommandType 枚举（PUT, DELETE）

  * [x] SubTask 1.3: 定义 Command 结构体

  * [x] SubTask 1.4: 定义 LogEntry 结构体

  * [x] SubTask 1.5: 添加必要的构造函数和默认值

* [x] Task 2: 实现日志管理器类

  * [x] SubTask 2.1: 定义 LogManager 类框架

  * [x] SubTask 2.2: 实现 append() 方法

  * [x] SubTask 2.3: 实现 get() 方法

  * [x] SubTask 2.4: 实现 last_log_index() 方法

  * [x] SubTask 2.5: 实现 last_log_term() 方法

  * [x] SubTask 2.6: 实现 truncate_from() 方法

  * [x] SubTask 2.7: 实现 get_entries_from() 方法

  * [x] SubTask 2.8: 实现 size() 和 empty() 方法

  * [x] SubTask 2.9: 添加线程安全支持（mutex）

* [x] Task 3: 定义 AppendEntries RPC

  * [x] SubTask 3.1: 创建 src/raft/append_entries.h 文件

  * [x] SubTask 3.2: 定义 AppendEntriesRequest 结构体

  * [x] SubTask 3.3: 定义 AppendEntriesResponse 结构体

  * [x] SubTask 3.4: 添加快速回滚优化字段

  * [x] SubTask 3.5: 添加必要的注释说明

* [ ] Task 4: 编写单元测试

  * [ ] SubTask 4.1: 编写 LogManager 基本操作测试

  * [ ] SubTask 4.2: 编写 LogManager 截断测试

  * [ ] SubTask 4.3: 编写 LogManager 边界条件测试

  * [ ] SubTask 4.4: 编写 AppendEntries RPC 结构测试

* [ ] Task 5: 更新 CMakeLists.txt

  * [ ] SubTask 5.1: 确保新头文件被正确包含

## Day 3-5 任务（已完成）

### Step 3: 实现 Leader 端日志复制逻辑

* [x] Task 6: 扩展 RaftNode 类添加日志相关字段

  * [x] SubTask 6.1: 添加 LogManager 成员变量

  * [x] SubTask 6.2: 添加 commit_index 成员变量

  * [x] SubTask 6.3: 添加 last_applied 成员变量

  * [x] SubTask 6.4: 添加 next_index 映射（Leader 专用）

  * [x] SubTask 6.5: 添加 match_index 映射（Leader 专用）

  * [x] SubTask 6.6: 添加相关 getter/setter 方法

* [x] Task 7: 实现 Leader 端日志复制核心方法

  * [x] SubTask 7.1: 实现 build_append_entries_request() 方法

    * 构建 AppendEntriesRequest，包含 prev_log_index、prev_log_term、entries

  * [x] SubTask 7.2: 实现 send_append_entries() 方法

    * 向指定 Follower 发送日志复制请求

  * [x] SubTask 7.3: 实现 handle_append_entries_response() 方法

    * 处理 Follower 的响应，更新 next_index 和 match_index

  * [x] SubTask 7.4: 实现 update_commit_index() 方法

    * 根据多数派确认更新 commit_index

* [x] Task 8: 实现 Leader 心跳机制

  * [x] SubTask 8.1: 实现心跳定时器逻辑

  * [x] SubTask 8.2: 实现发送心跳的方法（空 entries 的 AppendEntries）

  * [x] SubTask 8.3: 实现心跳响应处理

### Step 4: 实现 Follower 端日志追加和一致性检查

* [x] Task 9: 实现 Follower 端日志处理核心方法

  * [x] SubTask 9.1: 实现 handle_append_entries() 方法入口

    * 处理来自 Leader 的 AppendEntries RPC

  * [x] SubTask 9.2: 实现任期检查逻辑

    * 拒绝过期请求，更新自己的任期

  * [x] SubTask 9.3: 实现日志一致性检查

    * 检查 prev_log_index 和 prev_log_term 是否匹配

  * [x] SubTask 9.4: 实现日志追加和冲突处理

    * 处理日志冲突，删除不一致条目，追加新条目

  * [x] SubTask 9.5: 实现 commit_index 更新逻辑

    * 根据 leader_commit 更新本地 commit_index

* [x] Task 10: 实现快速回滚优化（可选）

  * [x] SubTask 10.1: 在响应中填充 conflict_index 和 conflict_term

  * [x] SubTask 10.2: Leader 端根据冲突信息快速回退 next_index

## Day 6-7 任务（后续执行）

* [ ] Task 11: 编写日志复制场景测试

  * [ ] SubTask 11.1: 编写正常日志复制测试

  * [ ] SubTask 11.2: 编写日志冲突测试

  * [ ] SubTask 11.3: 编写心跳测试

  * [ ] SubTask 11.4: 编写快速回滚测试

* [ ] Task 12: 编译和测试

  * [ ] SubTask 12.1: 执行编译命令

  * [ ] SubTask 12.2: 修复编译错误（如果有）

  * [ ] SubTask 12.3: 运行单元测试

  * [ ] SubTask 12.4: 修复测试失败（如果有）

  * [ ] SubTask 12.5: 确保所有测试通过

## 知识点讲解文档部分

* [x] Task 13: 创建知识点讲解文档

  * [x] SubTask 13.1: 创建 docs/DistributedKV_Guide/stageB/week8 目录

  * [x] SubTask 13.2: 创建 Week8_LogReplication.md 文件

  * [x] SubTask 13.3: 编写通俗理解部分

  * [x] SubTask 13.4: 编写专业术语部分

  * [x] SubTask 13.5: 编写代码实现指南

* [x] Task 14: 更新知识点讲解文档（Day 3-5）

  * [x] SubTask 14.1: 添加 Leader 端日志复制逻辑详解

  * [x] SubTask 14.2: 添加 Follower 端日志处理详解

  * [x] SubTask 14.3: 添加一致性检查机制详解

  * [x] SubTask 14.4: 添加代码实现示例

# Task Dependencies

### Day 1-2 依赖

* [Task 2] 依赖于 [Task 1] - 需要先定义 LogEntry 结构

* [Task 3] 可以与 [Task 1, Task 2] 并行执行

* [Task 4] 依赖于 [Task 1, Task 2, Task 3] - 需要先实现功能

* [Task 5] 依赖于所有代码实现任务

### Day 3-5 依赖

* [Task 7] 依赖于 [Task 6] - 需要先添加日志相关字段

* [Task 8] 依赖于 [Task 6] - 需要先添加日志相关字段

* [Task 9] 依赖于 [Task 6] - 需要先添加日志相关字段

* [Task 10] 依赖于 [Task 7, Task 9] - 需要先实现基本逻辑

### Day 6-7 依赖

* [Task 11] 依赖于 [Task 6, Task 7, Task 8, Task 9, Task 10] - 需要先实现功能

* [Task 12] 依赖于所有代码实现任务

### 整体依赖

* 文档任务可以与代码实现任务并行执行

* Day 3-5 任务必须等待 Day 1-2 任务完成

* Day 6-7 任务必须等待 Day 3-5 任务完成
