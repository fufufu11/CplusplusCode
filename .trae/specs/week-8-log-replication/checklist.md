# Checklist

## Day 1-2 检查项（已完成）

### 代码实现检查

#### LogEntry 结构检查

- [x] CommandType 枚举定义正确
  - [x] kPut 枚举值定义
  - [x] kDelete 枚举值定义

- [x] Command 结构体定义完整
  - [x] type 字段定义
  - [x] key 字段定义
  - [x] value 字段定义
  - [x] 默认构造函数
  - [x] 参数化构造函数

- [x] LogEntry 结构体定义完整
  - [x] term 字段定义
  - [x] index 字段定义
  - [x] command 字段定义
  - [x] 默认构造函数
  - [x] 参数化构造函数

#### LogManager 类检查

- [x] LogManager 类定义完整
  - [x] append() 方法实现正确
  - [x] get() 方法实现正确
  - [x] last_log_index() 方法实现正确
  - [x] last_log_term() 方法实现正确
  - [x] truncate_from() 方法实现正确
  - [x] get_entries_from() 方法实现正确
  - [x] size() 方法实现正确
  - [x] empty() 方法实现正确

- [x] 线程安全实现正确
  - [x] mutex 成员变量定义
  - [x] 所有公共方法加锁
  - [x] 使用 lock_guard 或 unique_lock

- [x] 索引处理正确
  - [x] 日志索引从1开始
  - [x] 占位符处理正确

#### AppendEntries RPC 检查

- [x] AppendEntriesRequest 结构体定义完整
  - [x] term 字段定义
  - [x] leader_id 字段定义
  - [x] prev_log_index 字段定义
  - [x] prev_log_term 字段定义
  - [x] entries 字段定义
  - [x] leader_commit 字段定义
  - [x] 默认构造函数

- [x] AppendEntriesResponse 结构体定义完整
  - [x] term 字段定义
  - [x] success 字段定义
  - [x] conflict_index 字段定义
  - [x] conflict_term 字段定义
  - [x] 默认构造函数

### 代码质量检查

- [x] 代码符合 C++20 标准
- [x] 使用 RAII 原则管理资源
- [x] 代码风格一致
- [x] 注释清晰完整
- [x] 变量命名规范
- [x] 无内存泄漏
- [x] 无未使用的变量

### 编译检查

- [x] 代码编译无错误
- [x] 代码编译无警告
- [x] CMakeLists.txt 配置正确

### 测试检查

- [ ] LogManager 基本操作测试通过
- [ ] LogManager 截断测试通过
- [ ] LogManager 边界条件测试通过
- [ ] AppendEntries RPC 结构测试通过

## Day 3-5 检查项（已完成）

### RaftNode 扩展检查

#### 日志相关字段检查

- [x] LogManager 成员变量正确添加
  - [x] log_manager_ 成员变量定义
  - [x] 通过 getter 方法访问

- [x] commit_index 字段正确添加
  - [x] commit_index_ 成员变量定义
  - [x] 初始值为 0
  - [x] getter/setter 方法实现

- [x] last_applied 字段正确添加
  - [x] last_applied_ 成员变量定义
  - [x] 初始值为 0
  - [x] getter/setter 方法实现

- [x] next_index 映射正确添加（Leader 专用）
  - [x] next_index_ 成员变量定义（std::map 或 std::unordered_map）
  - [x] become_leader() 中正确初始化
  - [x] getter/setter 方法实现

- [x] match_index 映射正确添加（Leader 专用）
  - [x] match_index_ 成员变量定义
  - [x] become_leader() 中正确初始化
  - [x] getter/setter 方法实现

### Leader 端日志复制逻辑检查

#### build_append_entries_request() 方法检查

- [x] 方法签名正确
- [x] 正确获取 prev_log_index（next_index_ - 1）
- [x] 正确获取 prev_log_term
- [x] 正确获取要发送的日志条目
- [x] 正确设置 leader_commit
- [x] 返回完整的 AppendEntriesRequest

#### send_append_entries() 方法检查

- [x] 方法签名正确
- [x] 正确构建请求
- [x] 正确处理发送结果

#### handle_append_entries_response() 方法检查

- [x] 方法签名正确
- [x] 正确处理成功响应
  - [x] 更新 match_index_
  - [x] 更新 next_index_
- [x] 正确处理失败响应
  - [x] 递减 next_index_
  - [x] 处理快速回滚优化（如果实现）
- [x] 正确处理任期更新

#### update_commit_index() 方法检查

- [x] 方法签名正确
- [x] 正确计算多数派确认的索引
- [x] 只提交当前任期的日志（安全性保证）
- [x] 正确更新 commit_index_

### Leader 心跳机制检查

- [x] 心跳定时器逻辑正确
- [x] 心跳间隔合理（通常 50ms）
- [x] 空 entries 的 AppendEntries 正确发送
- [x] 心跳响应正确处理

### Follower 端日志处理检查

#### handle_append_entries() 方法检查

- [x] 方法签名正确
- [x] 正确重置选举超时
- [x] 正确返回 AppendEntriesResponse

#### 任期检查逻辑检查

- [x] 拒绝过期请求（req.term < current_term_）
- [x] 更新任期（req.term > current_term_）
- [x] 转为 Follower 状态（如果需要）

#### 日志一致性检查检查

- [x] 正确检查 prev_log_index 存在性
- [x] 正确检查 prev_log_term 匹配
- [x] 正确返回失败响应（包含冲突信息）

#### 日志追加和冲突处理检查

- [x] 正确处理日志冲突
  - [x] 删除不一致的条目
  - [x] 追加新条目
- [x] 正确处理日志追加
- [x] 日志索引连续性保证

#### commit_index 更新逻辑检查

- [x] 正确比较 leader_commit 和本地 commit_index_
- [x] 正确更新 commit_index_
- [x] 不超过日志最后索引

### 快速回滚优化检查（可选）

- [x] Follower 正确填充 conflict_index
- [x] Follower 正确填充 conflict_term
- [x] Leader 正确使用冲突信息快速回退

### 代码质量检查

- [x] 代码符合 C++20 标准
- [x] 使用 RAII 原则管理资源
- [x] 代码风格一致
- [x] 注释清晰完整（Doxygen 风格）
- [x] 变量命名规范
- [x] 无内存泄漏
- [x] 无未使用的变量

### 编译检查

- [x] 代码编译无错误
- [x] 代码编译无警告

## Day 6-7 检查项（后续执行）

- [ ] 正常日志复制测试通过
- [ ] 日志冲突测试通过
- [ ] 心跳测试通过
- [ ] 快速回滚测试通过
- [ ] 所有单元测试通过
- [ ] 测试覆盖率达标

## 知识点文档检查

### 文档完整性检查

- [x] 通俗理解部分完整
  - [x] 生活类比清晰
  - [x] 流程说明详细
  - [x] 关键概念解释到位

- [x] 专业术语部分完整
  - [x] 术语定义准确
  - [x] 技术细节完整
  - [x] 代码示例可用

- [x] 代码实现指南完整
  - [x] 文件结构清晰
  - [x] 实现步骤明确
  - [x] 关键细节说明到位

- [x] 测试策略部分完整
  - [x] 测试用例覆盖全面
  - [x] 边界条件考虑充分

- [x] 常见问题部分完整
  - [x] 常见陷阱说明
  - [x] 解决方案提供

### Day 3-5 文档更新检查

- [x] Leader 端日志复制逻辑详解完整
  - [x] 方法说明清晰
  - [x] 流程图/伪代码提供
  - [x] 关键代码示例

- [x] Follower 端日志处理详解完整
  - [x] 方法说明清晰
  - [x] 流程图/伪代码提供
  - [x] 关键代码示例

- [x] 一致性检查机制详解完整
  - [x] 检查逻辑说明
  - [x] 冲突处理策略
  - [x] 代码示例

### 文档质量检查

- [x] 理论讲解准确无误
- [x] 代码示例符合 C++20 标准
- [x] 文档结构清晰易读
- [x] 术语使用一致
