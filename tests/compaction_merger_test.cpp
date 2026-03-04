#include <gtest/gtest.h>
#include "compaction_merger.h"
#include "sstable_builder.h"
#include "sstable_reader.h"
#include "value_type.h"

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {
    const std::string kTestDir = "./test_data_merger";
}

/**
 * @brief CompactionMerger 单元测试夹具
 *
 * 提供测试所需的辅助函数和资源管理。
 */
class CompactionMergerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(kTestDir);
    }

    void TearDown() override {
        readers_.clear();
        std::filesystem::remove_all(kTestDir);
    }

    /**
     * @brief 构建测试用 SSTable 文件
     *
     * @param filename SSTable 文件名（相对于测试目录）
     * @param data KV 数据映射，支持普通值和 Tombstone
     */
    void BuildSSTableWithData(const std::string& filename,
                              const std::map<std::string, Value>& data) {
        std::string filepath = kTestDir + "/" + filename;
        SSTableBuilder builder(filepath);
        for (const auto& [k, v] : data) {
            builder.Add(k, v);
        }
        builder.Finish();
    }

    /**
     * @brief 从 SSTable 文件创建迭代器
     *
     * @param filename SSTable 文件名（相对于测试目录）
     * @return std::unique_ptr<SSTableReader::Iterator> 迭代器实例
     *
     * @note SSTableReader 的所有权由 readers_ 成员持有，确保迭代器生命周期内有效
     */
    std::unique_ptr<SSTableReader::Iterator> CreateIterator(const std::string& filename) {
        std::string filepath = kTestDir + "/" + filename;
        readers_.push_back(std::make_unique<SSTableReader>(filepath));
        return readers_.back()->NewIterator();
    }

    /**
     * @brief 收集归并器中的所有 KV 对
     *
     * @param merger 归并器实例
     * @return std::vector<std::pair<std::string, Value>> 所有 KV 对列表
     */
    std::vector<std::pair<std::string, Value>> CollectAll(CompactionMerger& merger) {
        std::vector<std::pair<std::string, Value>> result;
        while (merger.Valid()) {
            result.emplace_back(merger.Key(), merger.GetValue());
            merger.Next();
        }
        return result;
    }

    /**
     * @brief 验证 KV 对列表是否按键升序排列
     *
     * @param entries KV 对列表
     * @return true 有序
     * @return false 无序
     */
    bool IsSorted(const std::vector<std::pair<std::string, Value>>& entries) {
        for (size_t i = 1; i < entries.size(); ++i) {
            if (entries[i].first <= entries[i - 1].first) {
                return false;
            }
        }
        return true;
    }

    std::vector<std::unique_ptr<SSTableReader>> readers_;
};

/**
 * @brief 测试合并 2 个有序 SSTable
 *
 * 验证合并后输出仍然有序。
 */
TEST_F(CompactionMergerTest, MergeTwoSSTables) {
    BuildSSTableWithData("sstable1.sst", {
        {"a", Value::normal("value_a1")},
        {"c", Value::normal("value_c1")},
        {"e", Value::normal("value_e1")}
    });

    BuildSSTableWithData("sstable2.sst", {
        {"b", Value::normal("value_b2")},
        {"d", Value::normal("value_d2")},
        {"f", Value::normal("value_f2")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("sstable1.sst"));
    iterators.push_back(CreateIterator("sstable2.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_TRUE(IsSorted(result));
    EXPECT_EQ(result.size(), 6);

    EXPECT_EQ(result[0].first, "a");
    EXPECT_EQ(result[1].first, "b");
    EXPECT_EQ(result[2].first, "c");
    EXPECT_EQ(result[3].first, "d");
    EXPECT_EQ(result[4].first, "e");
    EXPECT_EQ(result[5].first, "f");
}

/**
 * @brief 测试合并 4 个有序 SSTable
 *
 * 验证多路归并的正确性和有序性。
 */
TEST_F(CompactionMergerTest, MergeFourSSTables) {
    BuildSSTableWithData("sstable1.sst", {
        {"a", Value::normal("1")},
        {"i", Value::normal("1")}
    });

    BuildSSTableWithData("sstable2.sst", {
        {"c", Value::normal("2")},
        {"k", Value::normal("2")}
    });

    BuildSSTableWithData("sstable3.sst", {
        {"e", Value::normal("3")},
        {"m", Value::normal("3")}
    });

    BuildSSTableWithData("sstable4.sst", {
        {"g", Value::normal("4")},
        {"o", Value::normal("4")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("sstable1.sst"));
    iterators.push_back(CreateIterator("sstable2.sst"));
    iterators.push_back(CreateIterator("sstable3.sst"));
    iterators.push_back(CreateIterator("sstable4.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_TRUE(IsSorted(result));
    EXPECT_EQ(result.size(), 8);

    std::vector<std::string> expected_keys = {"a", "c", "e", "g", "i", "k", "m", "o"};
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i].first, expected_keys[i]);
    }
}

/**
 * @brief 测试重复 Key 的优先级处理
 *
 * 同一 Key 在多个 SSTable 中存在不同值时，
 * 应保留优先级高的版本（索引小，代表更新的数据）。
 */
TEST_F(CompactionMergerTest, DuplicateKeyPriority) {
    BuildSSTableWithData("sstable_new.sst", {
        {"key1", Value::normal("new_value1")},
        {"key2", Value::normal("new_value2")},
        {"key3", Value::normal("value3_new")}
    });

    BuildSSTableWithData("sstable_old.sst", {
        {"key1", Value::normal("old_value1")},
        {"key2", Value::normal("old_value2")},
        {"key4", Value::normal("value4_old")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("sstable_new.sst"));
    iterators.push_back(CreateIterator("sstable_old.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_EQ(result.size(), 4);

    auto find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "key1"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "new_value1");

    find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "key2"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "new_value2");

    find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "key3"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "value3_new");

    find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "key4"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "value4_old");
}

/**
 * @brief 测试 Tombstone 正确保留
 *
 * 验证删除标记（Tombstone）在没有新值覆盖时正确保留。
 */
TEST_F(CompactionMergerTest, TombstonePreserved) {
    BuildSSTableWithData("sstable_new.sst", {
        {"deleted_key", Value::tombstone()},
        {"normal_key", Value::normal("normal_value")}
    });

    BuildSSTableWithData("sstable_old.sst", {
        {"deleted_key", Value::normal("old_value")},
        {"another_key", Value::normal("another_value")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("sstable_new.sst"));
    iterators.push_back(CreateIterator("sstable_old.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_EQ(result.size(), 3);

    auto find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "deleted_key"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_TRUE(find_result->second.is_tombstone());

    find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "normal_key"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "normal_value");
    EXPECT_FALSE(find_result->second.is_tombstone());

    find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "another_key"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "another_value");
}

/**
 * @brief 测试 Tombstone 被新值覆盖
 *
 * Tombstone 后有新值时，验证输出新值而非 Tombstone。
 */
TEST_F(CompactionMergerTest, TombstoneOverwritten) {
    BuildSSTableWithData("sstable_newest.sst", {
        {"key1", Value::normal("newest_value1")},
        {"key2", Value::normal("newest_value2")}
    });

    BuildSSTableWithData("sstable_middle.sst", {
        {"key1", Value::tombstone()},
        {"key2", Value::tombstone()},
        {"key3", Value::tombstone()}
    });

    BuildSSTableWithData("sstable_oldest.sst", {
        {"key1", Value::normal("oldest_value1")},
        {"key2", Value::normal("oldest_value2")},
        {"key3", Value::normal("oldest_value3")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("sstable_newest.sst"));
    iterators.push_back(CreateIterator("sstable_middle.sst"));
    iterators.push_back(CreateIterator("sstable_oldest.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_EQ(result.size(), 3);

    auto find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "key1"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "newest_value1");
    EXPECT_FALSE(find_result->second.is_tombstone());

    find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "key2"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_EQ(find_result->second.data, "newest_value2");
    EXPECT_FALSE(find_result->second.is_tombstone());

    find_result = std::find_if(result.begin(), result.end(),
        [](const auto& p) { return p.first == "key3"; });
    ASSERT_NE(find_result, result.end());
    EXPECT_TRUE(find_result->second.is_tombstone());
}

/**
 * @brief 测试空输入
 *
 * 空迭代器列表时，验证 Valid() 返回 false。
 */
TEST_F(CompactionMergerTest, EmptyInput) {
    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;

    CompactionMerger merger(std::move(iterators));

    EXPECT_FALSE(merger.Valid());
}

/**
 * @brief 测试单个 SSTable
 *
 * 验证单个迭代器的正确处理。
 */
TEST_F(CompactionMergerTest, SingleSSTable) {
    BuildSSTableWithData("single.sst", {
        {"a", Value::normal("1")},
        {"b", Value::normal("2")},
        {"c", Value::normal("3")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("single.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_EQ(result.size(), 3);
    EXPECT_TRUE(IsSorted(result));

    EXPECT_EQ(result[0].first, "a");
    EXPECT_EQ(result[1].first, "b");
    EXPECT_EQ(result[2].first, "c");
}

/**
 * @brief 测试包含空迭代器的输入
 *
 * 验证空迭代器被正确跳过。
 */
TEST_F(CompactionMergerTest, NullIteratorsSkipped) {
    BuildSSTableWithData("valid.sst", {
        {"key", Value::normal("value")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(nullptr);
    iterators.push_back(CreateIterator("valid.sst"));
    iterators.push_back(nullptr);

    CompactionMerger merger(std::move(iterators));

    EXPECT_TRUE(merger.Valid());
    EXPECT_EQ(merger.Key(), "key");
    EXPECT_EQ(merger.GetValue().data, "value");
}

/**
 * @brief 测试所有 Key 相同的场景
 *
 * 多个 SSTable 中 Key 完全相同，验证只保留优先级最高的版本。
 */
TEST_F(CompactionMergerTest, AllSameKeys) {
    BuildSSTableWithData("sstable1.sst", {
        {"same_key", Value::normal("value1")}
    });

    BuildSSTableWithData("sstable2.sst", {
        {"same_key", Value::normal("value2")}
    });

    BuildSSTableWithData("sstable3.sst", {
        {"same_key", Value::normal("value3")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("sstable1.sst"));
    iterators.push_back(CreateIterator("sstable2.sst"));
    iterators.push_back(CreateIterator("sstable3.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].first, "same_key");
    EXPECT_EQ(result[0].second.data, "value1");
}

/**
 * @brief 测试合并 6 个有序 SSTable
 *
 * 验证 CompactionMerger 可以处理任意数量的 SSTable，
 * 与 Compaction 触发阈值无关。
 */
TEST_F(CompactionMergerTest, MergeSixSSTables) {
    BuildSSTableWithData("sstable1.sst", {
        {"a", Value::normal("1")},
        {"m", Value::normal("1")}
    });

    BuildSSTableWithData("sstable2.sst", {
        {"c", Value::normal("2")},
        {"o", Value::normal("2")}
    });

    BuildSSTableWithData("sstable3.sst", {
        {"e", Value::normal("3")},
        {"q", Value::normal("3")}
    });

    BuildSSTableWithData("sstable4.sst", {
        {"g", Value::normal("4")},
        {"s", Value::normal("4")}
    });

    BuildSSTableWithData("sstable5.sst", {
        {"i", Value::normal("5")},
        {"u", Value::normal("5")}
    });

    BuildSSTableWithData("sstable6.sst", {
        {"k", Value::normal("6")},
        {"w", Value::normal("6")}
    });

    std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
    iterators.push_back(CreateIterator("sstable1.sst"));
    iterators.push_back(CreateIterator("sstable2.sst"));
    iterators.push_back(CreateIterator("sstable3.sst"));
    iterators.push_back(CreateIterator("sstable4.sst"));
    iterators.push_back(CreateIterator("sstable5.sst"));
    iterators.push_back(CreateIterator("sstable6.sst"));

    CompactionMerger merger(std::move(iterators));

    auto result = CollectAll(merger);

    EXPECT_TRUE(IsSorted(result));
    EXPECT_EQ(result.size(), 12);

    std::vector<std::string> expected_keys = {"a", "c", "e", "g", "i", "k", "m", "o", "q", "s", "u", "w"};
    for (size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i].first, expected_keys[i]);
    }
}
