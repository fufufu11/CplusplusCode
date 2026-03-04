#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include "kv_store.h"
#include "wal_record.h"
#include "value_type.h"

namespace fs = std::filesystem;

class KVStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 每个测试开始前，确保数据目录是空的
        test_dir_ = "test_data_kvstore";
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    void TearDown() override {
        // 测试结束后清理（可选，为了观察结果也可以不清理，这里选择清理保持环境干净）
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    std::string test_dir_;
};

/**
 * @brief 测试数据目录的自动创建
 */
TEST_F(KVStoreTest, CreatesDataDirectory) {
    EXPECT_FALSE(fs::exists(test_dir_));
    
    {
        KVStore store(test_dir_);
        EXPECT_TRUE(fs::exists(test_dir_));
        EXPECT_TRUE(fs::is_directory(test_dir_));
    }
}

/**
 * @brief 测试基本的 Put/Get 流程
 */
TEST_F(KVStoreTest, BasicPutGet) {
    KVStore store(test_dir_);
    
    store.put(1, "one");
    store.put(2, "two");

    auto v1 = store.get(1);
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1.value(), "one");

    auto v2 = store.get(2);
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v2.value(), "two");

    EXPECT_FALSE(store.get(3).has_value());
}

/**
 * @brief 测试更新已存在的 Key
 */
TEST_F(KVStoreTest, UpdateKey) {
    KVStore store(test_dir_);
    store.put(1, "v1");
    EXPECT_EQ(store.get(1).value(), "v1");

    store.put(1, "v1_updated");
    EXPECT_EQ(store.get(1).value(), "v1_updated");
}

/**
 * @brief 测试删除逻辑
 * 
 * 注意：使用 Tombstone 机制后，del() 总是返回 true（写入 Tombstone），
 * 即使 key 不存在也是如此。这是 LSM-Tree 的标准行为。
 */
TEST_F(KVStoreTest, DeleteKey) {
    KVStore store(test_dir_);
    store.put(10, "ten");
    
    EXPECT_TRUE(store.del(10));
    EXPECT_FALSE(store.get(10).has_value());
    
    // 删除不存在的 key 也会成功（写入 Tombstone）
    // 这是 LSM-Tree 的标准行为
    EXPECT_TRUE(store.del(10));
}

/**
 * @brief 测试 WAL 文件检测（模拟）
 * 
 * 真正的 WAL 写入和重放将在后续任务实现，这里仅测试“KVStore 能感知到 WAL 文件的存在”
 * 并打印对应的日志（虽然 GTest 捕获 stdout 比较麻烦，但我们可以侧面验证没有崩溃）。
 */
TEST_F(KVStoreTest, DetectsExistingWAL) {
    // 1. 手动创建目录和 WAL 文件
    fs::create_directories(test_dir_);
    std::ofstream wal(fs::path(test_dir_) / "wal.log");
    wal << "dummy content";
    wal.close();

    // 2. 启动 Store，它应该检测到 WAL 并打印 "Found WAL file..."
    // (此处我们主要验证程序能正常启动且不报错)
    {
        KVStore store(test_dir_);
        // 验证目录和文件依然存在（没有被误删）
        EXPECT_TRUE(fs::exists(fs::path(test_dir_) / "wal.log"));
    }
}

/**
 * @brief 验证 WAL 写入持久化 (Task 3 验收)
 */
TEST_F(KVStoreTest, WALPersistenceCheck) {
    {
        KVStore store(test_dir_);
        store.put(1, "persistent_val");
        store.del(1);
    }
    
    // 验证 WAL 文件存在且不为空
    fs::path wal_path = fs::path(test_dir_) / "wal.log";
    ASSERT_TRUE(fs::exists(wal_path));
    EXPECT_GT(fs::file_size(wal_path), 0);
    
    // 简单的内容检查（确保里面有刚才写入的字符串）
    std::ifstream ifs(wal_path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    
    // 应该能找到 key 和 value 的痕迹
    // 注意：int key=1 被转为 string "1"
    EXPECT_NE(content.find("persistent_val"), std::string::npos);
    EXPECT_NE(content.find("1"), std::string::npos); // key "1"
}

/**
 * @brief [Task 4] WAL 重放恢复测试 (Recovery Test)
 */
class WALReplayTest : public KVStoreTest {
    // 继承 KVStoreTest 的 SetUp/TearDown
};

TEST_F(WALReplayTest, NormalRecovery) {
    // 1. 写入数据并销毁 Store
    {
        KVStore store(test_dir_);
        store.put(1, "val1");
        store.put(2, "val2");
        store.del(1); // 删除 1
    } // store 析构，确保 buffer flush 到文件

    // 2. 重新打开 Store (触发 Replay)
    {
        KVStore store(test_dir_);
        
        // 验证 key 1 被删除
        auto v1 = store.get(1);
        EXPECT_FALSE(v1.has_value());

        // 验证 key 2 存在
        auto v2 = store.get(2);
        ASSERT_TRUE(v2.has_value());
        EXPECT_EQ(v2.value(), "val2");
    }
}

TEST_F(WALReplayTest, TruncatedWAL) {
    // 1. 写入有效数据
    {
        KVStore store(test_dir_);
        store.put(1, "valid");
    }

    // 2. 模拟尾部截断：手动向 WAL 追加不足一个 Header 的垃圾数据
    fs::path wal_path = fs::path(test_dir_) / "wal.log";
    {
        std::ofstream wal(wal_path, std::ios::binary | std::ios::app);
        char trash[5] = {0, 1, 2, 3, 4};
        wal.write(trash, 5); // 写入 5 字节，不足 Header(13)
    }

    // 3. 重启，程序应忽略尾部垃圾，且保留之前的数据
    {
        KVStore store(test_dir_);
        auto v1 = store.get(1);
        ASSERT_TRUE(v1.has_value());
        EXPECT_EQ(v1.value(), "valid");
    }
}

TEST_F(WALReplayTest, CorruptedWAL) {
    // 1. 写入数据
    {
        KVStore store(test_dir_);
        store.put(1, "val1");
    }

    // 2. 破坏文件：修改中间的一个字节导致 Checksum 不匹配
    // 假设 put(1, "val1") 的编码长度大约是 13+1+4 = 18 字节左右
    fs::path wal_path = fs::path(test_dir_) / "wal.log";
    {
        std::fstream wal(wal_path, std::ios::binary | std::ios::in | std::ios::out);
        wal.seekp(10); // 偏移 10 字节处修改
        wal.put(static_cast<char>(0xFF)); // 破坏数据
    }

    // 3. 重启
    // 预期：Replay 检测到 Checksum 错误后停止。
    // 由于我们目前的策略是 "Stop Replay"，且被破坏的是第一条记录，
    // 所以最终结果是 MemTable 为空（第一条也没恢复进去）。
    {
        KVStore store(test_dir_);
        auto v1 = store.get(1);
        EXPECT_FALSE(v1.has_value()) << "Corrupted record should not be applied";
    }
}

TEST_F(WALReplayTest, BulkRecovery1000) {
    {
        KVStore store(test_dir_);
        for (int i = 0; i < 1000; ++i) {
            store.put(i, "v" + std::to_string(i));
        }
    }

    {
        KVStore store(test_dir_);
        for (int i = 0; i < 1000; ++i) {
            auto v = store.get(i);
            ASSERT_TRUE(v.has_value());
            EXPECT_EQ(v.value(), "v" + std::to_string(i));
        }
    }
}

TEST_F(WALReplayTest, MixedPutDelRecovery) {
    {
        KVStore store(test_dir_);
        for (int i = 0; i < 200; ++i) {
            store.put(i, "init" + std::to_string(i));
        }
        for (int i = 0; i < 200; i += 2) {
            store.del(i);
        }
        for (int i = 1; i < 200; i += 2) {
            store.put(i, "final" + std::to_string(i));
        }
    }

    {
        KVStore store(test_dir_);
        for (int i = 0; i < 200; ++i) {
            auto v = store.get(i);
            if (i % 2 == 0) {
                EXPECT_FALSE(v.has_value());
            } else {
                ASSERT_TRUE(v.has_value());
                EXPECT_EQ(v.value(), "final" + std::to_string(i));
            }
        }
    }
}

TEST_F(WALReplayTest, TruncateMidRecord) {
    {
        KVStore store(test_dir_);
        store.put(1, "v1");
        store.put(2, "v2");
    }

    fs::path wal_path = fs::path(test_dir_) / "wal.log";
    LogRecord r1{LogType::kPut, "00000000001", Value::normal("v1")};
    const auto r1_encoded = encode_log_record(r1);
    fs::resize_file(wal_path, r1_encoded.size() + 6);

    {
        KVStore store(test_dir_);
        auto v1 = store.get(1);
        ASSERT_TRUE(v1.has_value());
        EXPECT_EQ(v1.value(), "v1");

        auto v2 = store.get(2);
        EXPECT_FALSE(v2.has_value());
    }
}

TEST_F(WALReplayTest, CorruptMiddleRecordStopsAtPrefix) {
    {
        KVStore store(test_dir_);
        for (int i = 1; i <= 10; ++i) {
            store.put(i, "v" + std::to_string(i));
        }
    }

    fs::path wal_path = fs::path(test_dir_) / "wal.log";
    std::vector<size_t> record_sizes;
    record_sizes.reserve(10);
    for (int i = 1; i <= 10; ++i) {
        char key_buf[16];
        std::snprintf(key_buf, sizeof(key_buf), "%011d", i);
        LogRecord r{LogType::kPut, std::string(key_buf), Value::normal("v" + std::to_string(i))};
        record_sizes.push_back(encode_log_record(r).size());
    }

    size_t offset = 0;
    for (int i = 0; i < 4; ++i) {
        offset += record_sizes[i];
    }

    {
        std::fstream wal(wal_path, std::ios::binary | std::ios::in | std::ios::out);
        wal.seekp(static_cast<std::streamoff>(offset + 14));
        wal.put(static_cast<char>(0xFF));
    }

    {
        KVStore store(test_dir_);
        for (int i = 1; i <= 4; ++i) {
            auto v = store.get(i);
            ASSERT_TRUE(v.has_value());
            EXPECT_EQ(v.value(), "v" + std::to_string(i));
        }
        for (int i = 5; i <= 10; ++i) {
            EXPECT_FALSE(store.get(i).has_value());
        }
    }
}

/**
 * @brief Tombstone 测试：存储特殊字符串 "__tombstone__"
 * 
 * 验证用户可以存储任意字符串，包括 "__tombstone__"，
 * 而不会被误认为是删除标记。
 */
TEST_F(KVStoreTest, StoreSpecialStringTombstone) {
    KVStore store(test_dir_);
    
    store.put(1, "__tombstone__");
    
    auto result = store.get(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "__tombstone__");
}

/**
 * @brief Tombstone 测试：删除后查询返回 nullopt
 */
TEST_F(KVStoreTest, DeleteThenGetReturnsNullopt) {
    KVStore store(test_dir_);
    
    store.put(1, "value1");
    EXPECT_TRUE(store.get(1).has_value());
    
    store.del(1);
    EXPECT_FALSE(store.get(1).has_value());
}

/**
 * @brief Tombstone 测试：删除后重新写入返回新值
 */
TEST_F(KVStoreTest, DeleteThenRewriteReturnsNewValue) {
    KVStore store(test_dir_);
    
    store.put(1, "old_value");
    store.del(1);
    store.put(1, "new_value");
    
    auto result = store.get(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "new_value");
}

/**
 * @brief Tombstone 测试：重启后删除语义正确
 */
TEST_F(KVStoreTest, TombstonePersistsAfterRecovery) {
    {
        KVStore store(test_dir_);
        store.put(1, "value1");
        store.del(1);
        store.put(2, "value2");
    }
    
    {
        KVStore store(test_dir_);
        EXPECT_FALSE(store.get(1).has_value());
        
        auto result = store.get(2);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result.value(), "value2");
    }
}

/**
 * @brief Tombstone 测试：Flush 后删除语义正确
 */
TEST_F(KVStoreTest, TombstonePersistsAfterFlush) {
    KVStore store(test_dir_);
    
    store.put(1, "value1");
    store.Flush();
    
    EXPECT_TRUE(store.get(1).has_value());
    
    store.del(1);
    store.Flush();
    
    EXPECT_FALSE(store.get(1).has_value());
}

class CompactionTriggerTest : public KVStoreTest {
};

TEST_F(CompactionTriggerTest, GetL0FileCountReturnsCorrectCount) {
    KVStore store(test_dir_);
    
    EXPECT_EQ(store.GetL0FileCount(), 0);
    
    store.put(1, "v1");
    store.Flush();
    EXPECT_EQ(store.GetL0FileCount(), 1);
    
    store.put(2, "v2");
    store.Flush();
    EXPECT_EQ(store.GetL0FileCount(), 2);
    
    store.put(3, "v3");
    store.Flush();
    EXPECT_EQ(store.GetL0FileCount(), 3);
}

TEST_F(CompactionTriggerTest, ShouldCompactReturnsFalseBelowThreshold) {
    KVStore store(test_dir_);
    
    store.put(1, "v1");
    store.Flush();
    EXPECT_EQ(store.GetL0FileCount(), 1);
    
    store.put(2, "v2");
    store.Flush();
    EXPECT_EQ(store.GetL0FileCount(), 2);
    
    store.put(3, "v3");
    store.Flush();
    EXPECT_EQ(store.GetL0FileCount(), 3);
}

TEST_F(CompactionTriggerTest, ShouldCompactReturnsTrueAtThreshold) {
    KVStore store(test_dir_);
    
    for (int i = 0; i < 3; ++i) {
        store.put(i, "v" + std::to_string(i));
        store.Flush();
    }
    EXPECT_EQ(store.GetL0FileCount(), 3);
    
    store.put(4, "v4");
    store.Flush();
    
    EXPECT_EQ(store.GetL0FileCount(), 0);
    
    for (int i = 0; i < 3; ++i) {
        auto v = store.get(i);
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(v.value(), "v" + std::to_string(i));
    }
    auto v4 = store.get(4);
    ASSERT_TRUE(v4.has_value());
    EXPECT_EQ(v4.value(), "v4");
}

TEST_F(CompactionTriggerTest, CustomThresholdConfigWorks) {
    CompactionConfig config;
    config.l0_trigger_count = 2;
    
    KVStore store(test_dir_, config);
    
    store.put(1, "v1");
    store.Flush();
    EXPECT_EQ(store.GetL0FileCount(), 1);
    
    store.put(2, "v2");
    store.Flush();
    
    EXPECT_EQ(store.GetL0FileCount(), 0);
    
    auto v1 = store.get(1);
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(v1.value(), "v1");
    
    auto v2 = store.get(2);
    ASSERT_TRUE(v2.has_value());
    EXPECT_EQ(v2.value(), "v2");
}
