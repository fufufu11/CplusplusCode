# 第7周：Raft状态机框架实现与知识点讲解

## Why

第7周的任务是实现Raft状态机框架，这是整个Raft协议实现的基础。同时，为了帮助开发者深入理解相关概念，需要创建一份详细的技术文档，系统讲解Raft状态机的核心知识点。代码实现提供功能基础，文档提供理论支撑，两者相辅相成。

## What Changes

### 代码实现部分
- 创建Raft核心数据结构定义文件（raft_node.h）
- 实现RaftNode类，包含状态枚举和核心数据结构
- 实现状态转换方法（become_leader、become_follower、become_candidate）
- 实现选举超时检测和随机化逻辑
- 定义RequestVote RPC结构体
- 编写单元测试验证状态转换和超时机制

### 知识点讲解文档部分
- 创建技术文档文件（docs/DistributedKV_Guide/week7/Week7_RaftStateMachine.md）
- 系统讲解状态机基础理论
- 详细说明Raft三种状态及其职责
- 深入解析状态转换机制
- 讲解选举超时处理原理
- 说明RPC设计原则
- 提供代码示例和最佳实践

## Impact

- 受影响规范：无
- 受影响代码：
  - 新增：src/raft/raft_node.h、src/raft/raft_node.cc
  - 新增：src/raft/election.h
  - 新增：tests/raft_test.cc
- 新增文档：
  - 新增：docs/DistributedKV_Guide/week7/Week7_RaftStateMachine.md
- 依赖：需要阶段A的测试框架（GTest）

## ADDED Requirements

### Requirement: RaftNode类定义
系统 SHALL 提供RaftNode类，包含完整的状态枚举和核心数据结构

#### Scenario: 类定义
- **WHEN** 编译器处理raft_node.h
- **THEN** RaftNode类应包含：
  - 状态枚举（Leader、Follower、Candidate）
  - 持久化状态（current_term、voted_for）
  - 易失性状态（state、voted_count、election_timeout）
  - 状态转换方法声明

### Requirement: 状态转换实现
系统 SHALL 实现三种状态之间的正确转换逻辑

#### Scenario: Follower→Candidate转换
- **WHEN** Follower在选举超时后触发转换
- **THEN** 节点应：
  - current_term自增
  - state变为Candidate
  - voted_for设为自己
  - voted_count初始化为1
  - 重置选举超时

#### Scenario: Candidate→Leader转换
- **WHEN** Candidate获得多数票
- **THEN** 节点应：
  - state变为Leader
  - 初始化next_index和match_index数组

#### Scenario: 任意状态→Follower转换
- **WHEN** 节点发现更高term的Leader或RPC请求
- **THEN** 节点应：
  - state变为Follower
  - current_term更新为更高term
  - 重置选举超时

### Requirement: 选举超时机制
系统 SHALL 实现选举超时检测和随机化逻辑

#### Scenario: 超时检测
- **WHEN** Follower/Candidate超过选举超时时间未收到心跳或投票
- **THEN** 应触发选举超时事件

#### Scenario: 超时随机化
- **WHEN** 初始化或重置选举超时
- **THEN** 超时时间应在[150ms, 300ms]范围内随机选择

### Requirement: RequestVote RPC定义
系统 SHALL 定义RequestVote RPC结构体，包含所有必要字段

#### Scenario: RPC结构定义
- **WHEN** 定义RequestVote RPC
- **THEN** 应包含：
  - term（候选人的任期号）
  - candidate_id（请求投票的候选人ID）
  - last_log_index（候选人最后一条日志的索引）
  - last_log_term（候选人最后一条日志的任期号）

### Requirement: 单元测试
系统 SHALL 提供完整的单元测试覆盖状态转换和超时机制

#### Scenario: 状态转换测试
- **WHEN** 运行状态转换测试
- **THEN** 所有状态转换场景应通过验证

#### Scenario: 超时机制测试
- **WHEN** 运行超时机制测试
- **THEN** 超时检测和随机化应正确工作

### Requirement: 知识点讲解文档
系统 SHALL 提供详细的技术文档，讲解第7周的核心知识点

#### Scenario: 文档完整性
- **WHEN** 开发者阅读Week7_RaftStateMachine.md
- **THEN** 文档应包含：
  - 状态机基础理论
  - Raft三种状态详解
  - 状态转换机制
  - 选举超时处理
  - RPC设计原则
  - 代码示例和最佳实践

#### Scenario: 文档可读性
- **WHEN** 开发者学习文档内容
- **THEN** 应能够：
  - 理解状态机模式的优势
  - 掌握Raft三种状态的职责
  - 理解状态转换的条件和过程
  - 理解选举超时的必要性
  - 理解RPC的设计原则

## MODIFIED Requirements

无

## REMOVED Requirements

无
