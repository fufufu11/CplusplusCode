#pragma once

#include "sstable_reader.h"
#include "value_type.h"

#include <memory>
#include <queue>
#include <string>
#include <vector>

/**
 * @brief 迭代器条目，用于优先队列中的元素包装
 *
 * 包装 SSTable 迭代器，包含优先级信息和当前 KV 缓存。
 */
struct IteratorEntry {
    std::unique_ptr<SSTableReader::Iterator> iter;
    uint32_t priority;
    std::string current_key;
    Value current_value;

    /**
     * @brief 检查迭代器是否有效
     * @return true 迭代器有效且有当前数据
     */
    bool Valid() const { return iter && iter->Valid(); }
};

/**
 * @brief 迭代器条目比较器，用于优先队列
 *
 * 比较规则：
 * 1. Key 升序（最小堆，Key 小的在堆顶）
 * 2. Key 相同时，优先级降序（优先级数值小的先出堆）
 *
 * 由于 std::priority_queue 是最大堆，比较器返回 true 表示 a 的优先级低于 b，
 * 即 a 应该排在 b 后面。
 */
struct IteratorEntryCompare {
    bool operator()(const std::unique_ptr<IteratorEntry>& a,
                    const std::unique_ptr<IteratorEntry>& b) const {
        if (a->current_key != b->current_key) {
            return a->current_key > b->current_key;
        }
        return a->priority > b->priority;
    }
};

/**
 * @brief Compaction 多路归并器
 *
 * 用于 LSM-Tree Compaction 过程中的多路归并排序。将多个 SSTable 的迭代器
 * 合并为一个有序的输出流。
 *
 * 核心特性：
 * - 使用优先队列实现最小堆
 * - 支持重复 Key 去重（保留优先级更高的版本，即更新的数据）
 * - 遵循 RAII 原则管理资源
 *
 * 使用示例：
 * @code
 * std::vector<std::unique_ptr<SSTableReader::Iterator>> iters;
 * // ... 添加迭代器
 * CompactionMerger merger(std::move(iters));
 * while (merger.Valid()) {
 *     Process(merger.Key(), merger.GetValue());
 *     merger.Next();
 * }
 * @endcode
 *
 * @note 本类非线程安全，需在单线程环境下使用
 */
class CompactionMerger {
public:
    /**
     * @brief 构造函数：初始化多路归并器
     *
     * @param iterators SSTable 迭代器列表，索引越小优先级越高（代表越新的 SSTable）
     *
     * 构造流程：
     * 1. 将所有迭代器包装为 IteratorEntry
     * 2. 为每个迭代器分配优先级（索引越小优先级越高）
     * 3. 将有效的迭代器加入优先队列
     */
    explicit CompactionMerger(std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators) {
        heap_ = std::make_unique<MinHeap>();

        for (size_t i = 0; i < iterators.size(); ++i) {
            if (!iterators[i]) {
                continue;
            }

            auto entry = std::make_unique<IteratorEntry>();
            entry->iter = std::move(iterators[i]);
            entry->priority = static_cast<uint32_t>(i);

            entry->iter->SeekToFirst();
            if (entry->iter->Valid()) {
                entry->current_key = entry->iter->Key();
                entry->current_value = entry->iter->GetValue();
                heap_->push(std::move(entry));
            }
        }

        UpdateCurrent();
    }

    /**
     * @brief 析构函数
     *
     * 由于使用智能指针管理资源，无需手动释放
     */
    ~CompactionMerger() = default;

    CompactionMerger(const CompactionMerger&) = delete;
    CompactionMerger& operator=(const CompactionMerger&) = delete;

    CompactionMerger(CompactionMerger&&) = default;
    CompactionMerger& operator=(CompactionMerger&&) = default;

    /**
     * @brief 检查归并器是否有效
     *
     * @return true 当前指向有效的 KV 对
     * @return false 已遍历完毕
     */
    bool Valid() const { return valid_; }

    /**
     * @brief 获取当前 Key
     *
     * @note 调用前必须确保 Valid() 为 true
     * @return const std::string& 当前 Key 的常量引用
     */
    const std::string& Key() const { return current_key_; }

    /**
     * @brief 获取当前 Value
     *
     * @note 调用前必须确保 Valid() 为 true
     * @return const ::Value& 当前 Value 的常量引用
     */
    const ::Value& GetValue() const { return current_value_; }

    /**
     * @brief 移动到下一个 KV 对
     *
     * 核心逻辑：
     * 1. 弹出当前堆顶元素
     * 2. 推进该迭代器，如果有效则重新入堆
     * 3. 检查新堆顶，如果 Key 与当前 Key 相同则跳过（保留优先级更高的版本）
     * 4. 更新当前 KV 缓存
     */
    void Next() {
        if (!valid_ || !heap_ || heap_->empty()) {
            valid_ = false;
            return;
        }

        auto top = PopTop();
        if (!top) {
            valid_ = false;
            return;
        }

        top->iter->Next();
        if (top->iter->Valid()) {
            top->current_key = top->iter->Key();
            top->current_value = top->iter->GetValue();
            heap_->push(std::move(top));
        }

        SkipDuplicateKeys();

        UpdateCurrent();
    }

private:
    using MinHeap = std::priority_queue<std::unique_ptr<IteratorEntry>,
                                        std::vector<std::unique_ptr<IteratorEntry>>,
                                        IteratorEntryCompare>;

    std::unique_ptr<MinHeap> heap_;
    bool valid_ = false;
    std::string current_key_;
    ::Value current_value_;

    /**
     * @brief 弹出堆顶元素
     *
     * @return std::unique_ptr<IteratorEntry> 堆顶元素，如果堆为空返回 nullptr
     */
    std::unique_ptr<IteratorEntry> PopTop() {
        if (!heap_ || heap_->empty()) {
            return nullptr;
        }
        auto top = std::move(const_cast<std::unique_ptr<IteratorEntry>&>(heap_->top()));
        heap_->pop();
        return top;
    }

    /**
     * @brief 获取堆顶元素的 Key
     *
     * @return const std::string& 堆顶 Key，如果堆为空返回空字符串
     */
    const std::string& PeekKey() const {
        static const std::string empty_key;
        if (!heap_ || heap_->empty()) {
            return empty_key;
        }
        return heap_->top()->current_key;
    }

    /**
     * @brief 跳过与当前 Key 相同的所有条目
     *
     * 在 LSM-Tree 中，相同的 Key 可能存在于多个 SSTable 中。
     * 我们保留优先级最高（索引最小）的版本，跳过其他版本。
     */
    void SkipDuplicateKeys() {
        while (heap_ && !heap_->empty()) {
            const auto& top_key = heap_->top()->current_key;
            if (top_key != current_key_) {
                break;
            }

            auto entry = PopTop();
            if (!entry) {
                break;
            }

            entry->iter->Next();
            if (entry->iter->Valid()) {
                entry->current_key = entry->iter->Key();
                entry->current_value = entry->iter->GetValue();
                heap_->push(std::move(entry));
            }
        }
    }

    /**
     * @brief 更新当前 KV 缓存
     *
     * 从堆顶读取当前 Key 和 Value
     */
    void UpdateCurrent() {
        if (!heap_ || heap_->empty()) {
            valid_ = false;
            return;
        }

        const auto& top = heap_->top();
        current_key_ = top->current_key;
        current_value_ = top->current_value;
        valid_ = true;
    }
};
