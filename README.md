# DistributedKV: 从零手写分布式 KV 存储引擎

[![License](https://img.shields.io/badge/license-MIT-blue)]()
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()

> 注意：本项目目前处于开发阶段，是一个用于学习分布式系统核心原理（LSM-Tree, WAL, Raft）的教学型项目。

## 项目愿景

DistributedKV 旨在通过"手写核心组件"的方式，深度解析现代分布式数据库的底层原理。我们不追求极致的工业级性能，而是追求**代码的可读性**、**架构的清晰性**以及**核心机制的正确性**。

核心技术栈：
- **存储引擎**: LSM-Tree (Log-Structured Merge-Tree)
- **持久化**: Write-Ahead Log (WAL) with Strong Durability (fsync/_commit)
- **一致性**: Raft Consensus Algorithm (开发中)
- **语言标准**: Modern C++20

## 当前进度 (Status)

### 阶段 A：单机存储引擎 ✅

| 模块 | 功能 | 状态 | 说明 |
| :--- | :--- | :--- | :--- |
| **MemTable** | SkipList (跳表) | ✅ 完成 | 支持 Insert/Search/Remove，基于随机层数优化 |
| **Storage** | KVStore 骨架 | ✅ 完成 | 统一管理内存与磁盘资源，提供对外接口 |
| **Persistence** | WAL Writer | ✅ 完成 | 实现 `Write -> Flush -> Sync` 强持久化链路 |
| **Persistence** | WAL Reader | ✅ 完成 | 实现基于 Checksum 的崩溃恢复 (Crash Recovery) |
| **Storage** | SSTable Builder | ✅ 完成 | 实现 Data Block 写入、Index Block 构建、Footer 生成 |
| **Storage** | SSTable Reader | ✅ 完成 | 文件读取、Index 解析、迭代器接口、二分查找 |
| **Storage** | Flush 机制 | ✅ 完成 | MemTable 到 SSTable 的持久化流程，支持自动/手动触发 |
| **Storage** | Tombstone 机制 | ✅ 完成 | 删除标记与查询语义，跨 SSTable 生效 |
| **Storage** | Compaction | ✅ 完成 | L0 → L1 合并，多路归并排序，重复 Key 处理 |

### 阶段 B：Raft 共识协议 🚧

| 模块 | 功能 | 状态 | 说明 |
| :--- | :--- | :--- | :--- |
| **Raft** | 状态机框架 | ✅ 完成 | Leader/Candidate/Follower 状态转换，选举超时检测 |
| **Raft** | 选举机制 | ✅ 完成 | RequestVote RPC，投票逻辑，任期管理 |
| **Raft** | 日志复制 | 🚧 开发中 | AppendEntries RPC，日志一致性检查 |
| **Raft** | 安全性保障 | 📋 计划中 | 领导人完整性约束，提交规则 |

详细的学习路径与任务拆解请参考：
- [阶段 A 学习手册](docs/DistributedKV_Guide/chapters/)
- [阶段 B Raft 白皮书](docs/DistributedKV_Guide/stageB/StageB_01.md)

## 快速上手 (Quick Start)

### 环境要求
- **编译器**: GCC 10+ / MSVC 19.28+ (需支持 C++20)
- **构建工具**: CMake 3.15+
- **平台**: Windows (推荐使用 MinGW-w64) / Linux

### 构建与运行 (Windows PowerShell)

本项目提供了便捷的构建脚本 `build.ps1`，自动处理 CMake 配置与编译。

```powershell
# 1. 克隆仓库
git clone https://github.com/fufufu11/DistributedKV.git
cd DistributedKV

# 2. 一键编译
.\build.ps1

# 3. 运行单元测试 (推荐)
ctest --test-dir build --output-on-failure

# 4. 运行持久化 Demo 程序
.\build\bin\DistributedKV_bin.exe
```

### 性能基准测试 (Benchmarks)

项目包含一个针对 SkipList 的基准测试工具，可用于评估不同参数下的性能表现。

```powershell
# 运行基准测试 (默认参数: n=100000)
.\build\bin\benchmark_skiplist.exe

# 自定义参数运行
# --n: 键值对数量
# --reads: 读取次数
# --max-level: 跳表最大层数
.\build\bin\benchmark_skiplist.exe --n 1000000 --reads 500000 --max-level 20
```

## 目录结构

```text
DistributedKV/
├── docs/                       # 文档中心
│   └── DistributedKV_Guide/
│       ├── chapters/           # 阶段 A 学习手册（原理+任务）
│       ├── stageB/             # 阶段 B Raft 白皮书
│       │   ├── week7/          # 第7周：Raft 状态机框架
│       │   └── week8/          # 第8周：日志复制
│       └── Test_Record.md      # 测试验收记录
├── examples/                   # 示例与基准测试代码
│   ├── benchmark_skiplist.cpp
│   └── sstable_demo.cpp
├── include/                    # 头文件 (API 定义)
│   ├── kv_store.h              # 存储引擎入口 (含 WAL 恢复逻辑)
│   ├── skiplist.h              # 跳表实现
│   ├── wal_record.h            # WAL 格式定义
│   ├── sstable.h               # SSTable 文件结构定义
│   ├── sstable_builder.h       # SSTable 构造器
│   ├── sstable_reader.h        # SSTable 读取器
│   ├── compaction_merger.h     # Compaction 多路归并器
│   └── value_type.h            # ValueType 枚举 (NORMAL/TOMBSTONE)
├── src/                        # 源代码
│   ├── main.cpp                # 演示程序入口
│   ├── sstable_reader.cpp      # SSTable 读取器实现
│   └── raft/                   # Raft 共识模块
│       ├── raft_node.h         # Raft 节点状态机
│       ├── election.h          # 选举相关定义
│       ├── log_entry.h         # 日志条目定义
│       └── append_entries.h    # AppendEntries RPC
├── tests/                      # 单元测试 (GTest)
│   ├── skiplist_test.cpp
│   ├── kv_store_test.cpp
│   ├── wal_record_test.cpp
│   ├── sstable_builder_test.cpp
│   ├── sstable_reader_test.cpp
│   ├── flush_integration_test.cpp
│   ├── compaction_merger_test.cpp
│   └── raft_test.cc
└── build.ps1                   # 构建脚本
```

## 测试策略

本项目采用 **TDD (测试驱动开发)** 模式，确保每个核心组件都经过严格的单元测试验证。

### 测试覆盖范围

| 模块 | 测试内容 |
| :--- | :--- |
| **SkipList** | 顺序/随机插入、查询、删除与边界条件 |
| **WAL** | 编码正确性、Checksum 校验、断电恢复能力 |
| **SSTable** | 文件创建、Block 写入、Footer 校验、RAII 行为 |
| **Flush** | MemTable 到 SSTable 持久化、自动触发、WAL 清理 |
| **Compaction** | 多路归并排序、重复 Key 处理、Tombstone 处理 |
| **Raft** | 状态转换、选举超时、投票逻辑、性能指标 |

当前测试覆盖：**127 个测试用例全部通过**

查看详细测试记录：[Test_Record.md](docs/DistributedKV_Guide/Test_Record.md)

## 核心特性

### 1. LSM-Tree 存储架构
- **MemTable**: 基于跳表的内存表，支持 O(log n) 的增删查
- **SSTable**: 有序字符串表，支持二分查找和迭代器遍历
- **Compaction**: L0 → L1 合并，多路归并排序

### 2. 强持久化保证
- **WAL 预写日志**: Write → Flush → Sync 三阶段持久化
- **崩溃恢复**: 基于 CRC32 Checksum 的数据校验
- **Fail-Stop 语义**: 损坏数据自动截断，恢复有效前缀

### 3. Raft 共识协议 (开发中)
- **状态机框架**: Leader/Candidate/Follower 状态转换
- **选举机制**: 随机超时、投票逻辑、任期管理
- **日志复制**: AppendEntries RPC、一致性检查

## 贡献与反馈

这是一个开放的学习项目，欢迎提交 Issue 或 Pull Request 来改进代码或补充文档。

---
Happy Coding!
