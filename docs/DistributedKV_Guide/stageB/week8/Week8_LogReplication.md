# 第8周：日志复制机制详解

## 目录

1. [概述](#1-概述)
2. [通俗理解：日志复制就像"团队会议记录同步"](#2-通俗理解日志复制就像团队会议记录同步)
3. [专业术语：Raft日志复制机制](#3-专业术语raft日志复制机制)
4. [日志条目结构设计](#4-日志条目结构设计)
5. [AppendEntries RPC设计](#5-appendentries-rpc设计)
6. [Leader 端日志复制逻辑（Day 3-5）](#6-leader-端日志复制逻辑day-3-5)
7. [Follower 端日志处理（Day 3-5）](#7-follower-端日志处理day-3-5)
8. [一致性检查机制详解（Day 3-5）](#8-一致性检查机制详解day-3-5)
9. [代码实现指南](#9-代码实现指南)
10. [测试策略](#10-测试策略)
11. [常见问题与陷阱](#11-常见问题与陷阱)

---

## 1. 概述

本周我们将实现 Raft 协议中最核心的机制之一——**日志复制（Log Replication）**。日志复制是 Raft 实现分布式一致性的关键手段，它确保所有节点的状态机以相同的顺序执行相同的命令序列。

### 本周学习目标

- 理解日志条目的结构和作用
- 掌握 AppendEntries RPC 的设计原理
- 实现日志管理器（LogManager）
- 实现基本的日志复制流程
- **Day 3-5 新增**：实现 Leader 端和 Follower 端的日志复制逻辑
- **Day 3-5 新增**：理解并实现一致性检查机制

---

## 2. 通俗理解：日志复制就像"团队会议记录同步"

### 2.1 场景设定：团队会议记录

想象这样一个场景：一个五人小组需要共同维护一份重要的会议记录本。这本记录本记录了团队的所有重要决定，每个人都必须保持自己的副本与团队一致。

```
┌─────────────────────────────────────────────────────────────────────┐
│                    团队会议记录同步机制                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  问题：如何确保每个人的记录本内容完全一致？                           │
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  方案：组长负责制                                             │   │
│  │  ─────────────                                               │   │
│  │  1. 选出一位组长（Leader）                                    │   │
│  │  2. 所有新决定先由组长记录                                    │   │
│  │  3. 组长通知所有成员记录这个决定                              │   │
│  │  4. 多数成员确认后，决定正式生效                              │   │
│  │  5. 组长告诉大家"第N条决定已生效"                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 日志条目就像"会议记录的一条"

每条会议记录包含三个关键信息：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    会议记录条目结构                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  编号：第几条记录（索引 Index）                               │   │
│  │  日期：哪一天记录的（任期 Term）                              │   │
│  │  内容：具体的决定（命令 Command）                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  示例：                                                             │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  编号: 1    日期: 第1届    决定: 购买办公设备                   │   │
│  │  编号: 2    日期: 第1届    决定: 招聘新员工                     │   │
│  │  编号: 3    日期: 第2届    决定: 调整工作时间                   │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  关键洞察：                                                         │
│  - 编号确保顺序（每个人都知道第N条是什么）                          │
│  - 日期标识来源（知道是哪届组长做的决定）                           │
│  - 内容是核心（具体要执行什么操作）                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.3 日志复制流程就像"组长通知成员记录"

当有人向组长提议新决定时，整个流程如下：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    日志复制流程：团队决策类比                          │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  步骤 1：有人向组长提议新决定                                        │
│  ─────────────────────────────                                       │
│  生活类比：小王向组长提议"购买新电脑"                                │
│  技术语义：客户端向 Leader 发送写请求                                │
│                                                                     │
│  步骤 2：组长先记在自己的本子上（未生效）                             │
│  ─────────────────────────────                                       │
│  生活类比：组长写下"第N条：购买新电脑"，但还没确认生效               │
│  技术语义：Leader 将日志追加到本地日志（未提交状态）                  │
│                                                                     │
│  步骤 3：组长通知所有成员记录这个决定                                │
│  ─────────────────────────────                                       │
│  生活类比：组长发消息"请记录第N条：购买新电脑"                       │
│  技术语义：Leader 发送 AppendEntries RPC 到所有 Follower            │
│                                                                     │
│  步骤 4：成员检查并记录                                              │
│  ─────────────────────────────                                       │
│  生活类比：成员检查"我有没有第N-1条？一致吗？"                       │
│  技术语义：Follower 进行一致性检查，追加日志                         │
│                                                                     │
│  步骤 5：多数成员确认后，决定正式生效                                │
│  ─────────────────────────────                                       │
│  生活类比：3个人中2个确认了，组长宣布"第N条生效"                     │
│  技术语义：Leader 更新 commit_index，通知 Follower                  │
│                                                                     │
│  步骤 6：执行决定                                                    │
│  ─────────────────────────────                                       │
│  生活类比：大家开始执行"购买新电脑"                                  │
│  技术语义：将日志应用到状态机                                        │
│                                                                     │
│  步骤 7：告诉提议人"决定已生效"                                      │
│  ─────────────────────────────                                       │
│  生活类比：组长告诉小王"购买新电脑的决定已生效"                      │
│  技术语义：Leader 响应客户端                                         │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.4 为什么需要"一致性检查"？

想象这样一个场景：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    为什么需要一致性检查？                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  场景：成员小李因为生病请假，错过了几天的会议                         │
│                                                                     │
│  组长的记录本：                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  第1条  第2条  第3条  第4条  第5条                            │   │
│  │  购买  招聘  调整  扩建  培训                                 │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  小李的记录本：                                                      │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  第1条  第2条                                                 │   │
│  │  购买  招聘                                                   │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
│  组长通知小李"请记录第5条：培训"                                     │
│  小李检查：我没有第3条和第4条！                                      │
│  小李回复："记录失败，请从第3条开始"                                 │
│                                                                     │
│  这就是一致性检查的作用：确保日志的连续性和一致性                     │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. 专业术语：Raft日志复制机制

### 3.1 日志条目（Log Entry）

**定义**：日志条目是 Raft 中数据复制的基本单位，包含三个核心字段：

```cpp
struct LogEntry {
    uint64_t term;      // 任期号：标识该条目由哪个任期的 Leader 创建
    uint64_t index;     // 日志索引：条目在日志中的位置（从1开始）
    Command command;    // 命令：要应用到状态机的操作
};
```

**技术要点**：
- **term**：用于检测冲突和判断日志新旧
- **index**：单调递增，用于定位和引用日志条目
- **command**：具体的状态机操作（如 PUT、DELETE）

### 3.2 日志匹配属性（Log Matching Property）

Raft 保证以下两个关键属性：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    日志匹配属性                                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  属性 1：如果两个日志在相同索引处有相同任期号，                        │
│         则它们存储相同的命令。                                        │
│  ─────────────────────────────────────────────────────               │
│  推理：Leader 在一个任期内不会在相同索引创建多个日志条目              │
│                                                                     │
│  属性 2：如果两个日志在相同索引处有相同任期号，                        │
│         则该索引之前的所有日志条目都相同。                            │
│  ─────────────────────────────────────────────────────               │
│  推理：AppendEntries 的一致性检查强制 Follower 复制 Leader 的日志     │
│                                                                     │
│  数学证明（归纳法）：                                                │
│  - 基础情况：空日志满足属性                                          │
│  - 归纳步骤：如果日志在索引 i 处匹配，则 AppendEntries 确保          │
│             在索引 i+1 处也匹配                                      │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.3 AppendEntries RPC

**定义**：AppendEntries 是 Leader 用于复制日志条目和发送心跳的 RPC。

```cpp
struct AppendEntriesRequest {
    uint64_t term;                    // Leader 的任期号
    std::string leader_id;            // Leader 的 ID（用于客户端重定向）
    uint64_t prev_log_index;          // 紧邻新日志条目之前的日志索引
    uint64_t prev_log_term;           // prev_log_index 处日志的任期号
    std::vector<LogEntry> entries;    // 新日志条目（空表示心跳）
    uint64_t leader_commit;           // Leader 的 commit_index
};

struct AppendEntriesResponse {
    uint64_t term;          // 响应者的当前任期号
    bool success;           // 是否成功匹配 prev_log_index 和 prev_log_term
    
    // 快速回滚优化字段（可选）
    uint64_t conflict_index;    // 冲突条目的索引
    uint64_t conflict_term;     // 冲突条目的任期
};
```

**关键字段解释**：

| 字段 | 作用 | 生活类比 |
|------|------|----------|
| `term` | 检测过期 Leader | "我是第X届组长" |
| `prev_log_index` | 一致性检查点 | "你们应该有第N条记录" |
| `prev_log_term` | 验证日志一致性 | "那条记录是第Y届的" |
| `entries` | 要复制的日志 | "请添加这些新记录" |
| `leader_commit` | 已提交的日志索引 | "第M条已生效" |

### 3.4 一致性检查机制

Follower 收到 AppendEntries 后的检查流程：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Follower 一致性检查流程                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  1. 检查任期                                                        │
│     if (req.term < current_term_) {                                │
│         return {term: current_term_, success: false};              │
│     }                                                               │
│     // 拒绝过期请求                                                 │
│                                                                     │
│  2. 检查日志存在性                                                   │
│     if (log_.size() <= req.prev_log_index) {                       │
│         return {term: current_term_, success: false};              │
│     }                                                               │
│     // 日志不够长，缺少之前的条目                                    │
│                                                                     │
│  3. 检查日志任期匹配                                                 │
│     if (log_[req.prev_log_index].term != req.prev_log_term) {      │
│         return {term: current_term_, success: false};              │
│     }                                                               │
│     // 任期不匹配，存在冲突                                         │
│                                                                     │
│  4. 追加日志                                                        │
│     // 处理冲突：删除不匹配的条目                                    │
│     // 追加新条目                                                   │
│                                                                     │
│  5. 更新 commit_index                                               │
│     if (req.leader_commit > commit_index_) {                       │
│         commit_index_ = min(req.leader_commit, log_.size() - 1);   │
│     }                                                               │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 4. 日志条目结构设计

### 4.1 Command 结构

```cpp
// src/raft/log_entry.h

#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace raft {

/**
 * @brief 命令类型枚举。
 */
enum class CommandType : uint8_t {
    kPut = 0,     ///< PUT 操作：设置键值对
    kDelete = 1   ///< DELETE 操作：删除键
};

/**
 * @brief 状态机命令结构体。
 */
struct Command {
    CommandType type;
    std::string key;
    std::string value;    // 仅 PUT 操作使用
    
    Command() = default;
    
    Command(CommandType t, std::string k, std::string v = "")
        : type(t), key(std::move(k)), value(std::move(v)) {}
};

/**
 * @brief 日志条目结构体。
 */
struct LogEntry {
    uint64_t term;      ///< 任期号
    uint64_t index;     ///< 日志索引（从1开始）
    Command command;    ///< 状态机命令
    
    LogEntry() : term(0), index(0) {}
    
    LogEntry(uint64_t t, uint64_t idx, Command cmd)
        : term(t), index(idx), command(std::move(cmd)) {}
};

} // namespace raft
```

### 4.2 LogManager 类设计

```cpp
// src/raft/log_entry.h（续）

#include <vector>
#include <optional>
#include <mutex>

namespace raft {

/**
 * @brief 日志管理器类。
 * 
 * 负责管理日志条目的存储、检索和截断操作。
 * 线程安全。
 */
class LogManager {
public:
    LogManager() = default;
    ~LogManager() = default;
    
    // 禁止拷贝
    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;
    
    // 允许移动
    LogManager(LogManager&&) = default;
    LogManager& operator=(LogManager&&) = default;
    
    /**
     * @brief 追加日志条目。
     * @param entry 要追加的日志条目。
     */
    void append(const LogEntry& entry);
    
    /**
     * @brief 获取日志条目。
     * @param index 日志索引（从1开始）。
     * @return 日志条目，如果不存在返回 nullopt。
     */
    [[nodiscard]] std::optional<LogEntry> get(uint64_t index) const;
    
    /**
     * @brief 获取最后一个日志条目的索引。
     * @return 最后一个日志条目的索引，如果日志为空返回0。
     */
    [[nodiscard]] uint64_t last_log_index() const;
    
    /**
     * @brief 获取最后一个日志条目的任期。
     * @return 最后一个日志条目的任期，如果日志为空返回0。
     */
    [[nodiscard]] uint64_t last_log_term() const;
    
    /**
     * @brief 截断日志从指定索引开始。
     * @param from_index 起始索引（包含）。
     */
    void truncate_from(uint64_t from_index);
    
    /**
     * @brief 获取日志条目数量。
     * @return 日志条目数量。
     */
    [[nodiscard]] size_t size() const;
    
    /**
     * @brief 检查日志是否为空。
     * @return 如果日志为空返回 true。
     */
    [[nodiscard]] bool empty() const;
    
    /**
     * @brief 获取从指定索引开始的所有日志条目。
     * @param start_index 起始索引（从1开始）。
     * @return 日志条目列表。
     */
    [[nodiscard]] std::vector<LogEntry> get_entries_from(uint64_t start_index) const;

private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> log_;    // log_[0] 为占位符，实际日志从 log_[1] 开始
};

} // namespace raft
```

---

## 5. AppendEntries RPC设计

### 5.1 RPC 结构定义

```cpp
// src/raft/append_entries.h

#pragma once

#include "log_entry.h"
#include <cstdint>
#include <string>
#include <vector>

namespace raft {

/**
 * @brief AppendEntries RPC 请求结构体。
 * 
 * 由 Leader 发送，用于复制日志条目和发送心跳。
 */
struct AppendEntriesRequest {
    uint64_t term;                    ///< Leader 的任期号
    std::string leader_id;            ///< Leader 的 ID
    uint64_t prev_log_index;          ///< 紧邻新日志条目之前的日志索引
    uint64_t prev_log_term;           ///< prev_log_index 处日志的任期号
    std::vector<LogEntry> entries;    ///< 新日志条目（空表示心跳）
    uint64_t leader_commit;           ///< Leader 的 commit_index
    
    AppendEntriesRequest() 
        : term(0), prev_log_index(0), prev_log_term(0), leader_commit(0) {}
};

/**
 * @brief AppendEntries RPC 响应结构体。
 * 
 * Follower 对 AppendEntries 请求的响应。
 */
struct AppendEntriesResponse {
    uint64_t term;              ///< 响应者的当前任期号
    bool success;               ///< 是否成功匹配 prev_log_index 和 prev_log_term
    
    // 快速回滚优化字段
    uint64_t conflict_index;    ///< 冲突条目的索引
    uint64_t conflict_term;     ///< 冲突条目的任期
    
    AppendEntriesResponse() 
        : term(0), success(false), conflict_index(0), conflict_term(0) {}
};

} // namespace raft
```

---

## 6. Leader 端日志复制逻辑（Day 3-5）

### 6.1 Leader 需要维护的状态

Leader 除了基本状态外，还需要维护以下日志相关状态：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Leader 专用状态                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  next_index_（每个 Follower 一个）：                                 │
│  ─────────────────────────────                                       │
│  含义：对于每个 Follower，下一个要发送的日志索引                      │
│  生活类比：组长记录"小李已经收到第N条，下次从第N+1条开始发"           │
│  初始化：成为 Leader 时，初始化为 last_log_index + 1                 │
│                                                                     │
│  match_index_（每个 Follower 一个）：                                │
│  ─────────────────────────────────                                   │
│  含义：对于每个 Follower，已知复制的最高日志索引                      │
│  生活类比：组长记录"小李确认收到了第M条"                             │
│  初始化：成为 Leader 时，初始化为 0                                  │
│  更新：Follower 成功响应后更新                                       │
│                                                                     │
│  commit_index_：                                                     │
│  ─────────────                                                       │
│  含义：已知的最高已提交日志索引                                      │
│  生活类比：组长宣布"第K条决定已生效"                                 │
│  更新：多数派确认后更新                                              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.2 Leader 日志复制流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Leader 日志复制流程                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  步骤 1：接收客户端请求                                              │
│  ─────────────────────                                               │
│  • 客户端发送写请求到 Leader                                         │
│  • Leader 将请求封装为日志条目                                       │
│  • 追加到本地日志（未提交状态）                                       │
│                                                                     │
│  步骤 2：构建 AppendEntries 请求                                     │
│  ─────────────────────────────                                       │
│  • 获取目标 Follower 的 next_index                                  │
│  • 设置 prev_log_index = next_index - 1                             │
│  • 设置 prev_log_term = log[prev_log_index].term                    │
│  • 获取从 next_index 开始的所有日志条目                              │
│  • 设置 leader_commit = commit_index                                │
│                                                                     │
│  步骤 3：发送请求到 Follower                                         │
│  ─────────────────────────────                                       │
│  • 发送 AppendEntries RPC                                           │
│  • 等待响应                                                         │
│                                                                     │
│  步骤 4：处理响应                                                    │
│  ─────────────────────                                               │
│  • 如果成功：                                                        │
│    - 更新 match_index[peer] = prev_log_index + entries.size()      │
│    - 更新 next_index[peer] = match_index[peer] + 1                 │
│    - 尝试更新 commit_index                                          │
│  • 如果失败：                                                        │
│    - 递减 next_index[peer]                                          │
│    - 重试发送                                                       │
│                                                                     │
│  步骤 5：更新 commit_index                                           │
│  ─────────────────────────                                           │
│  • 找出满足以下条件的最大索引 N：                                    │
│    - N > commit_index                                               │
│    - log[N].term == current_term                                    │
│    - 多数派 match_index[peer] >= N                                  │
│  • 更新 commit_index = N                                            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.3 代码实现示例

```cpp
/**
 * @brief 构建 AppendEntries 请求。
 * 
 * @param peer_id 目标 Follower 的 ID。
 * @return AppendEntriesRequest 构建好的请求。
 */
AppendEntriesRequest RaftNode::build_append_entries_request(const std::string& peer_id) {
    AppendEntriesRequest req;
    req.term = current_term_;
    req.leader_id = node_id_;
    req.leader_commit = commit_index_;
    
    uint64_t next_idx = next_index_[peer_id];
    req.prev_log_index = next_idx - 1;
    
    if (req.prev_log_index > 0) {
        auto prev_entry = log_manager_.get(req.prev_log_index);
        if (prev_entry.has_value()) {
            req.prev_log_term = prev_entry->term;
        }
    }
    
    req.entries = log_manager_.get_entries_from(next_idx);
    
    return req;
}

/**
 * @brief 处理 AppendEntries 响应。
 * 
 * @param peer_id Follower 的 ID。
 * @param req 发送的请求。
 * @param resp 收到的响应。
 */
void RaftNode::handle_append_entries_response(
    const std::string& peer_id,
    const AppendEntriesRequest& req,
    const AppendEntriesResponse& resp) {
    
    if (resp.term > current_term_) {
        become_follower(resp.term);
        return;
    }
    
    if (resp.success) {
        match_index_[peer_id] = req.prev_log_index + req.entries.size();
        next_index_[peer_id] = match_index_[peer_id] + 1;
        update_commit_index();
    } else {
        if (resp.conflict_index > 0) {
            next_index_[peer_id] = resp.conflict_index;
        } else {
            next_index_[peer_id] = std::max(1ULL, next_index_[peer_id] - 1);
        }
    }
}

/**
 * @brief 更新 commit_index。
 */
void RaftNode::update_commit_index() {
    for (uint64_t n = log_manager_.last_log_index(); n > commit_index_; --n) {
        auto entry = log_manager_.get(n);
        if (!entry.has_value() || entry->term != current_term_) {
            continue;
        }
        
        size_t count = 1;
        for (const auto& [peer, match_idx] : match_index_) {
            if (match_idx >= n) {
                ++count;
            }
        }
        
        if (count > (cluster_size_ / 2)) {
            commit_index_ = n;
            break;
        }
    }
}
```

### 6.4 心跳机制

```
┌─────────────────────────────────────────────────────────────────────┐
│                    心跳机制                                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  目的：                                                             │
│  ──────                                                             │
│  1. 维护 Leader 的权威性（防止 Follower 发起选举）                   │
│  2. 同步 commit_index 到 Follower                                   │
│                                                                     │
│  实现方式：                                                          │
│  ──────────                                                          │
│  • Leader 定期发送空的 AppendEntries 请求                           │
│  • 心跳间隔通常为选举超时的 1/10 到 1/5                             │
│  • 典型值：heartbeat_interval = 50ms                                │
│                                                                     │
│  心跳请求特点：                                                      │
│  ─────────────                                                       │
│  • entries 为空                                                     │
│  • 仍然包含 prev_log_index、prev_log_term、leader_commit            │
│  • Follower 处理方式与普通日志复制相同                              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 7. Follower 端日志处理（Day 3-5）

### 7.1 Follower 处理 AppendEntries 的流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Follower 处理 AppendEntries 流程                  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  步骤 1：任期检查                                                    │
│  ─────────────────────                                               │
│  • 如果 req.term < current_term_：拒绝请求                          │
│  • 如果 req.term > current_term_：更新任期，转为 Follower            │
│  • 重置选举超时计时器                                                │
│                                                                     │
│  步骤 2：日志一致性检查                                              │
│  ─────────────────────                                               │
│  • 如果 prev_log_index > 0：                                        │
│    - 检查本地是否有 prev_log_index 处的日志                         │
│    - 检查该日志的任期是否等于 prev_log_term                         │
│  • 如果检查失败：返回失败响应，包含冲突信息                          │
│                                                                     │
│  步骤 3：日志追加                                                    │
│  ─────────────────────                                               │
│  • 对于每个新日志条目：                                              │
│    - 如果本地已有该索引的日志：                                      │
│      - 如果任期相同：跳过（已存在）                                  │
│      - 如果任期不同：删除从此处开始的所有日志，追加新条目            │
│    - 如果本地没有该索引的日志：追加新条目                            │
│                                                                     │
│  步骤 4：更新 commit_index                                           │
│  ─────────────────────────                                           │
│  • 如果 leader_commit > commit_index_：                             │
│    - commit_index_ = min(leader_commit, last_log_index)             │
│                                                                     │
│  步骤 5：返回成功响应                                                │
│  ─────────────────────                                               │
│  • 设置 term = current_term_                                        │
│  • 设置 success = true                                              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 7.2 代码实现示例

```cpp
/**
 * @brief 处理 AppendEntries 请求。
 * 
 * @param req AppendEntries 请求。
 * @return AppendEntriesResponse 响应。
 */
AppendEntriesResponse RaftNode::handle_append_entries(const AppendEntriesRequest& req) {
    AppendEntriesResponse resp;
    resp.term = current_term_;
    
    // 步骤 1：任期检查
    if (req.term < current_term_) {
        resp.success = false;
        return resp;
    }
    
    if (req.term > current_term_) {
        current_term_ = req.term;
        voted_for_ = std::nullopt;
    }
    
    become_follower(current_term_);
    update_heartbeat();
    
    // 步骤 2：日志一致性检查
    if (req.prev_log_index > 0) {
        auto prev_entry = log_manager_.get(req.prev_log_index);
        
        if (!prev_entry.has_value()) {
            resp.success = false;
            resp.conflict_index = log_manager_.last_log_index() + 1;
            return resp;
        }
        
        if (prev_entry->term != req.prev_log_term) {
            resp.success = false;
            resp.conflict_term = prev_entry->term;
            resp.conflict_index = find_first_index_of_term(resp.conflict_term);
            return resp;
        }
    }
    
    // 步骤 3：日志追加
    for (const auto& entry : req.entries) {
        auto existing = log_manager_.get(entry.index);
        
        if (existing.has_value()) {
            if (existing->term != entry.term) {
                log_manager_.truncate_from(entry.index);
                log_manager_.append(entry);
            }
        } else {
            log_manager_.append(entry);
        }
    }
    
    // 步骤 4：更新 commit_index
    if (req.leader_commit > commit_index_) {
        commit_index_ = std::min(req.leader_commit, log_manager_.last_log_index());
    }
    
    resp.success = true;
    return resp;
}

/**
 * @brief 找到指定任期的第一个日志索引。
 * 
 * @param term 任期号。
 * @return uint64_t 第一个日志索引，如果没有返回 0。
 */
uint64_t RaftNode::find_first_index_of_term(uint64_t term) const {
    for (uint64_t i = 1; i <= log_manager_.last_log_index(); ++i) {
        auto entry = log_manager_.get(i);
        if (entry.has_value() && entry->term == term) {
            return i;
        }
    }
    return 0;
}
```

---

## 8. 一致性检查机制详解（Day 3-5）

### 8.1 一致性检查的目的

```
┌─────────────────────────────────────────────────────────────────────┐
│                    一致性检查的目的                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  保证日志匹配属性：                                                  │
│  ────────────────                                                    │
│  1. 如果两个日志在相同索引处有相同任期号，则存储相同命令              │
│  2. 如果两个日志在相同索引处有相同任期号，则之前的日志都相同          │
│                                                                     │
│  这确保了：                                                          │
│  ──────────                                                          │
│  • 所有节点的日志最终一致                                            │
│  • 状态机以相同顺序执行相同命令                                      │
│  • 已提交的数据不会丢失                                              │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 8.2 一致性检查的工作原理

```
┌─────────────────────────────────────────────────────────────────────┐
│                    一致性检查工作原理                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  场景：Leader 向落后的 Follower 发送日志                             │
│                                                                     │
│  Leader 日志：                                                       │
│  索引:  1    2    3    4    5                                       │
│  任期:  1    1    2    2    2                                       │
│  命令:  A    B    C    D    E                                       │
│                                                                     │
│  Follower 日志（落后）：                                             │
│  索引:  1    2                                                       │
│  任期:  1    1                                                       │
│  命令:  A    B                                                       │
│                                                                     │
│  第一次请求：                                                        │
│  ─────────────                                                       │
│  Leader 发送：prev_log_index=4, prev_log_term=2, entries=[E]        │
│  Follower 检查：我没有索引 4 的日志！                                │
│  Follower 响应：success=false, conflict_index=3                     │
│                                                                     │
│  第二次请求：                                                        │
│  ─────────────                                                       │
│  Leader 发送：prev_log_index=2, prev_log_term=1, entries=[C,D,E]    │
│  Follower 检查：索引 2 的任期是 1，匹配！                            │
│  Follower 追加：[C,D,E]                                             │
│  Follower 响应：success=true                                        │
│                                                                     │
│  最终 Follower 日志：                                                │
│  索引:  1    2    3    4    5                                       │
│  任期:  1    1    2    2    2                                       │
│  命令:  A    B    C    D    E                                       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 8.3 日志冲突处理

```
┌─────────────────────────────────────────────────────────────────────┐
│                    日志冲突处理                                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  场景：Follower 的日志与 Leader 冲突                                 │
│                                                                     │
│  Leader 日志：                                                       │
│  索引:  1    2    3    4                                            │
│  任期:  1    1    2    2                                            │
│  命令:  A    B    C    D                                            │
│                                                                     │
│  Follower 日志（冲突）：                                             │
│  索引:  1    2    3    4                                            │
│  任期:  1    1    1    1    ← 索引 3-4 任期不同！                   │
│  命令:  A    B    X    Y                                            │
│                                                                     │
│  处理过程：                                                          │
│  ──────────                                                          │
│  1. Leader 发送：prev_log_index=2, prev_log_term=1, entries=[C,D]  │
│  2. Follower 检查：索引 2 匹配                                       │
│  3. Follower 处理 entries：                                         │
│     - 索引 3：本地任期 1 ≠ 新任期 2 → 删除索引 3-4，追加 C          │
│     - 索引 4：本地无此日志 → 追加 D                                 │
│                                                                     │
│  最终 Follower 日志：                                                │
│  索引:  1    2    3    4                                            │
│  任期:  1    1    2    2                                            │
│  命令:  A    B    C    D                                            │
│                                                                     │
│  关键规则：Leader 的日志具有权威性，Follower 必须服从                │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 8.4 快速回滚优化

```
┌─────────────────────────────────────────────────────────────────────┐
│                    快速回滚优化                                       │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  问题：逐个回退效率低                                                │
│  ─────────────────────                                               │
│  如果 Follower 缺少大量日志，Leader 需要多次重试                    │
│                                                                     │
│  优化方案：                                                          │
│  ──────────                                                          │
│  Follower 在失败响应中提供冲突信息：                                 │
│  • conflict_term：冲突条目的任期                                    │
│  • conflict_index：该任期的第一个日志索引                           │
│                                                                     │
│  Leader 使用冲突信息：                                               │
│  ────────────────────                                                │
│  1. 如果 Leader 有 conflict_term 的日志：                           │
│     - next_index = 该任期最后一个日志索引 + 1                       │
│  2. 如果 Leader 没有 conflict_term 的日志：                         │
│     - next_index = conflict_index                                   │
│                                                                     │
│  效果：一次 RPC 即可定位到正确的位置                                 │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 9. 代码实现指南

### 9.1 文件结构

```
src/raft/
├── log_entry.h          # LogEntry、Command、LogManager 定义
├── append_entries.h     # AppendEntries RPC 结构定义
├── election.h           # RequestVote RPC（已存在）
└── raft_node.h          # RaftNode 类（需要扩展）
```

### 9.2 RaftNode 扩展

需要在 RaftNode 类中添加以下成员：

```cpp
class RaftNode {
public:
    // ... 现有成员 ...

private:
    // 日志相关状态
    LogManager log_manager_;
    uint64_t commit_index_ = 0;
    uint64_t last_applied_ = 0;
    
    // Leader 专用状态
    std::unordered_map<std::string, uint64_t> next_index_;
    std::unordered_map<std::string, uint64_t> match_index_;
};
```

### 9.3 实现步骤

#### Step 3: Leader 端日志复制逻辑

1. 扩展 RaftNode 类添加日志相关字段
2. 实现 build_append_entries_request() 方法
3. 实现 send_append_entries() 方法
4. 实现 handle_append_entries_response() 方法
5. 实现 update_commit_index() 方法
6. 实现心跳机制

#### Step 4: Follower 端日志追加和一致性检查

1. 实现 handle_append_entries() 方法入口
2. 实现任期检查逻辑
3. 实现日志一致性检查
4. 实现日志追加和冲突处理
5. 实现 commit_index 更新逻辑

---

## 10. 测试策略

### 10.1 单元测试用例

```cpp
// tests/raft_test.cc

// LogManager 测试
TEST(LogManagerTest, AppendAndGet) {
    LogManager log;
    
    // 初始状态
    EXPECT_EQ(log.last_log_index(), 0);
    EXPECT_EQ(log.last_log_term(), 0);
    EXPECT_TRUE(log.empty());
    
    // 追加日志
    LogEntry entry1(1, 1, Command(CommandType::kPut, "key1", "value1"));
    log.append(entry1);
    
    EXPECT_EQ(log.last_log_index(), 1);
    EXPECT_EQ(log.last_log_term(), 1);
    EXPECT_FALSE(log.empty());
    
    // 获取日志
    auto retrieved = log.get(1);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->term, 1);
    EXPECT_EQ(retrieved->index, 1);
    EXPECT_EQ(retrieved->command.key, "key1");
    
    // 获取不存在的日志
    auto not_found = log.get(100);
    EXPECT_FALSE(not_found.has_value());
}

TEST(LogManagerTest, Truncate) {
    LogManager log;
    
    // 追加多条日志
    for (int i = 1; i <= 5; ++i) {
        log.append(LogEntry(1, i, Command(CommandType::kPut, "key" + std::to_string(i), "value")));
    }
    
    EXPECT_EQ(log.size(), 5);
    
    // 截断
    log.truncate_from(3);
    EXPECT_EQ(log.size(), 2);
    EXPECT_EQ(log.last_log_index(), 2);
}

TEST(LogManagerTest, GetEntriesFrom) {
    LogManager log;
    
    for (int i = 1; i <= 5; ++i) {
        log.append(LogEntry(1, i, Command(CommandType::kPut, "key" + std::to_string(i), "value")));
    }
    
    auto entries = log.get_entries_from(3);
    EXPECT_EQ(entries.size(), 3);
    EXPECT_EQ(entries[0].index, 3);
    EXPECT_EQ(entries[1].index, 4);
    EXPECT_EQ(entries[2].index, 5);
}

// AppendEntries RPC 测试
TEST(AppendEntriesTest, RequestStructure) {
    AppendEntriesRequest req;
    req.term = 1;
    req.leader_id = "leader1";
    req.prev_log_index = 0;
    req.prev_log_term = 0;
    req.leader_commit = 0;
    
    LogEntry entry(1, 1, Command(CommandType::kPut, "key1", "value1"));
    req.entries.push_back(entry);
    
    EXPECT_EQ(req.term, 1);
    EXPECT_EQ(req.entries.size(), 1);
}

TEST(AppendEntriesTest, ResponseStructure) {
    AppendEntriesResponse resp;
    resp.term = 1;
    resp.success = true;
    resp.conflict_index = 0;
    resp.conflict_term = 0;
    
    EXPECT_EQ(resp.term, 1);
    EXPECT_TRUE(resp.success);
}
```

### 10.2 测试覆盖率目标

- LogManager 类：100% 方法覆盖
- AppendEntries RPC 结构：100% 字段覆盖
- 边界条件：空日志、单条日志、大量日志

---

## 11. 常见问题与陷阱

### 11.1 索引从0还是从1开始？

**问题**：Raft 论文中日志索引从1开始，但 C++ 数组索引从0开始。

**解决方案**：
```cpp
// 方案1：使用占位符
std::vector<LogEntry> log_;
log_.push_back(LogEntry{});  // log_[0] 为占位符
// 实际日志从 log_[1] 开始

// 方案2：索引转换
uint64_t internal_index = log_index - 1;
```

**推荐**：使用占位符方案，代码更清晰。

### 11.2 日志截断时的内存管理

**问题**：截断日志后，内存是否立即释放？

**解决方案**：
```cpp
void truncate_from(uint64_t from_index) {
    log_.resize(from_index);
    // 可选：收缩内存
    log_.shrink_to_fit();
}
```

### 11.3 线程安全

**问题**：LogManager 可能被多个线程访问。

**解决方案**：
```cpp
class LogManager {
private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> log_;
    
public:
    void append(const LogEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_.push_back(entry);
    }
    // ... 其他方法类似
};
```

### 11.4 任期为0的日志条目

**问题**：如何区分"空日志"和"有效日志"？

**解决方案**：
```cpp
bool is_valid_entry(const LogEntry& entry) {
    return entry.term > 0 && entry.index > 0;
}
```

### 11.5 commit_index 更新的安全性

**问题**：为什么只能提交当前任期的日志？

**原因**：
```
┌─────────────────────────────────────────────────────────────────────┐
│                    为什么只能提交当前任期的日志？                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  场景：                                                             │
│  • S1 是 Term 2 的 Leader，日志 [1, 1, 2]                          │
│  • S2 有日志 [1, 1, 2, 2]                                          │
│  • S1 崩溃，S5 成为 Term 3 的 Leader，覆盖了 S2 的日志              │
│  • S5 的日志变成 [1, 1, 3]                                         │
│  • 如果 S1 恢复并成为 Term 4 的 Leader                              │
│  • 如果允许提交之前任期的日志，已提交的数据可能丢失！                │
│                                                                     │
│  解决方案：                                                         │
│  只提交当前任期的日志，通过当前任期日志的提交间接提交之前的日志      │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 总结

本周我们完成了日志复制机制的基础设施：

1. **LogEntry 结构**：定义了日志条目的核心数据结构
2. **LogManager 类**：实现了日志的存储、检索和截断操作
3. **AppendEntries RPC**：定义了日志复制的通信协议
4. **Day 3-5 新增**：
   - Leader 端日志复制逻辑
   - Follower 端日志处理逻辑
   - 一致性检查机制
   - 快速回滚优化

这些是后续实现完整日志复制流程的基础。在接下来的 Day 6-7 中，我们将编写测试用例并验证实现的正确性。

---

## 参考资料

- [Raft Paper Section 5.3](https://raft.github.io/raft.pdf)
- [Log Replication Visualization](https://raft.github.io/#logreplication)
- [Raft GitHub](https://github.com/raft/raft)
