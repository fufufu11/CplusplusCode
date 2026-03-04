# Week 4 Task 1: SSTable 物理布局设计 Spec

## Why
在实现 SSTable 读写代码之前，我们需要先明确其在磁盘上的物理存储格式。
用户希望通过“亲自动手敲代码”的方式进行学习，因此本阶段重点在于**文档引导**和**辅助检查**，而非直接生成最终代码。

## What Changes
1.  **文档化 (AI 负责)**: AI 将在 `docs/DistributedKV_Guide/Learning_Manual.md` 中新增章节，详细图解 Data Block, Index Block 和 Footer 的布局。
2.  **代码实现 (用户负责)**: 用户参考手册和 AI 提供的示例，在 `include/sstable.h` 中手动实现核心结构体定义。

## Impact
- **Affected Specs**: Week 4 整体规划。
- **Affected Code**: 
    - 修改 `docs/DistributedKV_Guide/Learning_Manual.md`
    - 新增 `include/sstable.h` (由用户创建)

## ADDED Requirements

### Requirement: 教学式文档
手册必须包含清晰的字节级布局图示，以及每个字段的含义解释，足以指导用户写出对应的 C++ 结构体。

### Requirement: 用户主导的实现
AI 不直接创建 `include/sstable.h`。AI 应在对话中提供参考代码（Reference Code），由用户在 IDE 中手动输入并保存。

## MODIFIED Requirements
无。
