#include <gtest/gtest.h>
#include "kv_store.h"

#include <filesystem>
#include <string>

namespace {
    const std::string kTestDir = "./test_data_flush";
}

class FlushIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::remove_all(kTestDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(kTestDir);
    }
};

TEST_F(FlushIntegrationTest, ManualFlushCreatesSSTable) {
    {
        KVStore store(kTestDir);
        
        for (int i = 0; i < 100; ++i) {
            store.put(i, "value_" + std::to_string(i));
        }
        
        EXPECT_EQ(store.GetSSTableCount(), 0);
        
        store.Flush();
        
        EXPECT_EQ(store.GetSSTableCount(), 1);
    }
    
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(kTestDir) / "L0_1.sst"));
}

TEST_F(FlushIntegrationTest, ReadFromFlushedSSTable) {
    {
        KVStore store(kTestDir);
        
        for (int i = 0; i < 100; ++i) {
            store.put(i, "value_" + std::to_string(i));
        }
        
        store.Flush();
    }
    
    {
        KVStore store(kTestDir);
        
        for (int i = 0; i < 100; ++i) {
            auto result = store.get(i);
            EXPECT_TRUE(result.has_value()) << "Key " << i << " should exist";
            EXPECT_EQ(result.value(), "value_" + std::to_string(i));
        }
    }
}

TEST_F(FlushIntegrationTest, AutoFlushTriggered) {
    KVStore store(kTestDir);
    
    size_t large_value_size = 100 * 1024;
    std::string large_value(large_value_size, 'x');
    
    for (int i = 0; i < 30; ++i) {
        store.put(i, large_value);
    }
    
    EXPECT_GE(store.GetSSTableCount(), 1) << "Auto-flush should have been triggered";
}

TEST_F(FlushIntegrationTest, MultipleFlushes) {
    KVStore store(kTestDir);
    
    for (int flush = 0; flush < 3; ++flush) {
        for (int i = 0; i < 50; ++i) {
            int key = flush * 100 + i;
            store.put(key, "value_" + std::to_string(key));
        }
        store.Flush();
    }
    
    EXPECT_EQ(store.GetSSTableCount(), 3);
    
    for (int flush = 0; flush < 3; ++flush) {
        for (int i = 0; i < 50; ++i) {
            int key = flush * 100 + i;
            auto result = store.get(key);
            EXPECT_TRUE(result.has_value()) << "Key " << key << " should exist";
            EXPECT_EQ(result.value(), "value_" + std::to_string(key));
        }
    }
}

TEST_F(FlushIntegrationTest, MemTablePriorityOverSSTable) {
    KVStore store(kTestDir);
    
    store.put(1, "old_value");
    store.Flush();
    
    store.put(1, "new_value");
    
    auto result = store.get(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "new_value");
}

TEST_F(FlushIntegrationTest, RecoveryAfterFlush) {
    {
        KVStore store(kTestDir);
        
        for (int i = 0; i < 100; ++i) {
            store.put(i, "value_" + std::to_string(i));
        }
        
        store.Flush();
        
        for (int i = 100; i < 150; ++i) {
            store.put(i, "value_" + std::to_string(i));
        }
    }
    
    {
        KVStore store(kTestDir);
        
        EXPECT_EQ(store.GetSSTableCount(), 1);
        
        for (int i = 0; i < 150; ++i) {
            auto result = store.get(i);
            EXPECT_TRUE(result.has_value()) << "Key " << i << " should exist after recovery";
            EXPECT_EQ(result.value(), "value_" + std::to_string(i));
        }
    }
}

TEST_F(FlushIntegrationTest, WALClearedAfterFlush) {
    {
        KVStore store(kTestDir);
        
        for (int i = 0; i < 100; ++i) {
            store.put(i, "value_" + std::to_string(i));
        }
        
        store.Flush();
    }
    
    auto wal_path = std::filesystem::path(kTestDir) / "wal.log";
    EXPECT_TRUE(std::filesystem::exists(wal_path));
    
    uintmax_t wal_size = std::filesystem::file_size(wal_path);
    EXPECT_EQ(wal_size, 0) << "WAL should be empty after flush";
}

TEST_F(FlushIntegrationTest, LargeDatasetRoundTrip) {
    const int kNumEntries = 100;
    
    {
        KVStore store(kTestDir);
        
        for (int i = 0; i < kNumEntries; ++i) {
            store.put(i, "value_" + std::to_string(i) + "_data");
        }
        
        EXPECT_EQ(store.GetSSTableCount(), 0);
        store.Flush();
        EXPECT_EQ(store.GetSSTableCount(), 1);
    }
    
    {
        KVStore store(kTestDir);
        
        for (int i = 0; i < kNumEntries; ++i) {
            auto result = store.get(i);
            EXPECT_TRUE(result.has_value()) << "Key " << i << " should exist";
            if (result.has_value()) {
                EXPECT_EQ(result.value(), "value_" + std::to_string(i) + "_data");
            }
        }
    }
}

TEST_F(FlushIntegrationTest, DeleteAfterFlush) {
    KVStore store(kTestDir);
    
    store.put(1, "value1");
    store.put(2, "value2");
    store.Flush();
    
    EXPECT_TRUE(store.get(1).has_value());
    EXPECT_TRUE(store.get(2).has_value());
    
    store.put(3, "value3");
    EXPECT_TRUE(store.del(3));
    
    EXPECT_TRUE(store.get(1).has_value());
    EXPECT_TRUE(store.get(2).has_value());
    EXPECT_FALSE(store.get(3).has_value());
}

TEST_F(FlushIntegrationTest, UpdateAfterFlush) {
    KVStore store(kTestDir);
    
    store.put(1, "old_value");
    store.Flush();
    
    store.put(1, "new_value");
    
    auto result = store.get(1);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "new_value");
}

TEST_F(FlushIntegrationTest, MultiSSTableVersionPriority) {
    KVStore store(kTestDir);
    
    store.put(1, "v1_in_sst1");
    store.Flush();
    
    store.put(1, "v2_in_sst2");
    store.Flush();
    
    store.put(1, "v3_in_sst3");
    store.Flush();
    
    EXPECT_EQ(store.GetSSTableCount(), 3);
    
    auto result = store.get(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "v3_in_sst3");
}

TEST_F(FlushIntegrationTest, MultiSSTableWithTombstone) {
    KVStore store(kTestDir);
    
    store.put(1, "value_in_sst1");
    store.Flush();
    
    store.put(1, "value_in_sst2");
    store.Flush();
    
    store.del(1);
    store.Flush();
    
    EXPECT_EQ(store.GetSSTableCount(), 3);
    
    EXPECT_FALSE(store.get(1).has_value());
}

TEST_F(FlushIntegrationTest, MultiSSTableRecovery) {
    {
        KVStore store(kTestDir);
        
        store.put(1, "v1");
        store.Flush();
        
        store.put(1, "v2");
        store.Flush();
        
        store.put(2, "key2_value");
        store.Flush();
    }
    
    {
        KVStore store(kTestDir);
        
        EXPECT_EQ(store.GetSSTableCount(), 3);
        
        auto result1 = store.get(1);
        ASSERT_TRUE(result1.has_value());
        EXPECT_EQ(result1.value(), "v2");
        
        auto result2 = store.get(2);
        ASSERT_TRUE(result2.has_value());
        EXPECT_EQ(result2.value(), "key2_value");
    }
}
