# Tasks

## 实现任务列表

### 代码实现部分

* [x] Task 1: 创建Raft核心数据结构

  * [x] SubTask 1.1: 创建src/raft/raft\_node.h文件

  * [x] SubTask 1.2: 定义State枚举（Leader、Follower、Candidate）

  * [x] SubTask 1.3: 定义RaftNode类

  * [x] SubTask 1.4: 定义持久化状态字段（current\_term、voted\_for）

  * [x] SubTask 1.5: 定义易失性状态字段（state、voted\_count、election\_timeout）

  * [x] SubTask 1.6: 声明状态转换方法（become\_leader、become\_follower、become\_candidate）

  * [x] SubTask 1.7: 声明超时检测相关方法（reset\_election\_timeout、check\_election\_timeout）

* [x] Task 2: 实现状态转换逻辑

  * [x] SubTask 2.1: 创建src/raft/raft\_node.cc文件

  * [x] SubTask 2.2: 实现become\_follower()方法

  * [x] SubTask 2.3: 实现become\_candidate()方法

  * [x] SubTask 2.4: 实现become\_leader()方法

  * [x] SubTask 2.5: 实现状态转换时的日志输出

* [x] Task 3: 实现选举超时机制

  * [x] SubTask 3.1: 实现随机数生成器（用于超时随机化）

  * [x] SubTask 3.2: 实现reset\_election\_timeout()方法

  * [x] SubTask 3.3: 实现check\_election\_timeout()方法

  * [x] SubTask 3.4: 实现超时时间计算逻辑（150ms-300ms随机）

* [x] Task 4: 定义RequestVote RPC

  * [x] SubTask 4.1: 创建src/raft/election.h文件

  * [x] SubTask 4.2: 定义RequestVoteRequest结构体

  * [x] SubTask 4.3: 定义RequestVoteResponse结构体

  * [x] SubTask 4.4: 添加必要的注释说明字段含义

* [x] Task 5: 编写单元测试

  * [x] SubTask 5.1: 创建tests/raft\_test.cc文件

  * [x] SubTask 5.2: 编写状态转换测试用例

  * [x] SubTask 5.3: 编写选举超时测试用例

  * [x] SubTask 5.4: 编写边界条件测试用例

  * [x] SubTask 5.5: 确保测试覆盖率达标

* [x] Task 6: 更新CMakeLists.txt

  * [x] SubTask 6.1: 添加raft源文件到编译目标

  * [x] SubTask 6.2: 添加raft\_test到测试目标

  * [x] SubTask 6.3: 确保编译配置正确

* [x] Task 7: 编译和测试

  * [x] SubTask 7.1: 执行编译命令

  * [x] SubTask 7.2: 修复编译错误（如果有）

  * [x] SubTask 7.3: 运行单元测试

  * [x] SubTask 7.4: 修复测试失败（如果有）

  * [x] SubTask 7.5: 确保所有测试通过

### 知识点讲解文档部分

* [x] Task 8: 创建知识点讲解文档框架

  * [x] SubTask 8.1: 创建docs/DistributedKV\_Guide/week7目录

  * [x] SubTask 8.2: 创建Week7\_RaftStateMachine.md文件

  * [x] SubTask 8.3: 编写文档目录结构

  * [x] SubTask 8.4: 编写文档概述和目标

* [x] Task 9: 编写状态机基础理论章节

  * [x] SubTask 9.1: 编写状态机模式概述

  * [x] SubTask 9.2: 讲解状态机在分布式系统中的应用

  * [x] SubTask 9.3: 对比状态机模式与其他并发控制模式

  * [x] SubTask 9.4: 提供状态机模式的优势分析

* [x] Task 10: 编写Raft三种状态详解章节

  * [x] SubTask 10.1: 编写Leader状态详解

  * [x] SubTask 10.2: 编写Follower状态详解

  * [x] SubTask 10.3: 编写Candidate状态详解

  * [x] SubTask 10.4: 总结三种状态的职责对比

  * [x] SubTask 10.5: 提供状态转换图

* [x] Task 11: 编写状态转换机制章节

  * [x] SubTask 11.1: 详细说明Follower→Candidate转换

  * [x] SubTask 11.2: 详细说明Candidate→Leader转换

  * [x] SubTask 11.3: 详细说明任意状态→Follower转换

  * [x] SubTask 11.4: 讲解状态转换的触发条件

  * [x] SubTask 11.5: 讲解状态转换后的行为

* [x] Task 12: 编写选举超时处理章节

  * [x] SubTask 12.1: 讲解选举超时的作用和必要性

  * [x] SubTask 12.2: 详细说明超时随机化机制

  * [x] SubTask 12.3: 提供超时参数选择建议

  * [x] SubTask 12.4: 讲解超时重置策略

  * [x] SubTask 12.5: 提供超时机制实现示例

* [x] Task 13: 编写RPC设计章节

  * [x] SubTask 13.1: 讲解RequestVote RPC的设计原则

  * [x] SubTask 13.2: 详细说明RPC字段含义

  * [x] SubTask 13.3: 讲解RPC交互流程

  * [x] SubTask 13.4: 提供RPC实现示例

* [x] Task 14: 编写代码示例和最佳实践章节

  * [x] SubTask 14.1: 提供状态枚举定义示例

  * [x] SubTask 14.2: 提供状态转换方法示例

  * [x] SubTask 14.3: 提供超时检测实现示例

  * [x] SubTask 14.4: 提供最佳实践建议

  * [x] SubTask 14.5: 提供常见陷阱和注意事项

* [x] Task 15: 编写数据结构设计章节

  * [x] SubTask 15.1: 讲解RaftNode核心数据结构

  * [x] SubTask 15.2: 说明持久化状态字段

  * [x] SubTask 15.3: 说明易失性状态字段

  * [x] SubTask 15.4: 提供数据结构设计示例

* [x] Task 16: 编写测试策略章节

  * [x] SubTask 16.1: 讲解状态转换测试策略

  * [x] SubTask 16.2: 讲解超时机制测试策略

  * [x] SubTask 16.3: 讲解边界条件测试

  * [x] SubTask 16.4: 提供测试用例示例

## Task Dependencies

### 代码实现部分依赖

* \[Task 2] 依赖于 \[Task 1] - 需要先定义数据结构

* \[Task 3] 依赖于 \[Task 1] - 需要先定义数据结构

* \[Task 4] 可以与 \[Task 1] 并行执行

* \[Task 5] 依赖于 \[Task 2, Task 3, Task 4] - 需要先实现功能

* \[Task 6] 依赖于 \[Task 1, Task 2, Task 3, Task 4] - 需要先有源文件

* \[Task 7] 依赖于所有代码实现任务 - 需要完整实现后才能编译测试

### 文档部分依赖

* \[Task 9] 可以独立执行

* \[Task 10] 可以独立执行

* \[Task 11] 依赖于 \[Task 10] - 需要先理解三种状态

* \[Task 12] 可以独立执行

* \[Task 13] 可以独立执行

* \[Task 14] 依赖于 \[Task 10, Task 11, Task 12, Task 13] - 需要综合前面知识

* \[Task 15] 可以与 \[Task 10] 并行执行

* \[Task 16] 依赖于所有其他文档任务 - 需要综合所有知识

### 整体依赖

* 文档任务可以与代码实现任务并行执行

* Task 7（编译测试）必须等待所有代码实现任务完成

