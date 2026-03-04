#include <gtest/gtest.h>
#include "sstable_builder.h"
#include "sstable_reader.h"
#include "value_type.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace {
    const std::string kTestDir = "./test_data_reader";
    const std::string kTestFile = kTestDir + "/test_reader.sst";
}

class SSTableReaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(kTestDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(kTestDir);
    }

    void BuildSSTable(const std::map<std::string, std::string>& data) {
        SSTableBuilder builder(kTestFile);
        for (const auto& [k, v] : data) {
            builder.Add(k, Value::normal(v));
        }
        builder.Finish();
    }
};

TEST_F(SSTableReaderTest, OpenValidFile) {
    std::map<std::string, std::string> data = {{"key1", "value1"}};
    BuildSSTable(data);

    EXPECT_NO_THROW({
        SSTableReader reader(kTestFile);
    });
}

TEST_F(SSTableReaderTest, OpenNonExistentFile) {
    EXPECT_THROW({
        SSTableReader reader("./nonexistent.sst");
    }, std::runtime_error);
}

TEST_F(SSTableReaderTest, GetSingleKey) {
    std::map<std::string, std::string> data = {{"key1", "value1"}};
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto result = reader.Get("key1");
    
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value1");
}

TEST_F(SSTableReaderTest, GetNonExistentKey) {
    std::map<std::string, std::string> data = {{"key1", "value1"}};
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto result = reader.Get("nonexistent");
    
    EXPECT_FALSE(result.has_value());
}

TEST_F(SSTableReaderTest, GetMultipleKeys) {
    std::map<std::string, std::string> data = {
        {"key1", "value1"},
        {"key2", "value2"},
        {"key3", "value3"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    
    for (const auto& [k, v] : data) {
        auto result = reader.Get(k);
        EXPECT_TRUE(result.has_value()) << "Key: " << k;
        EXPECT_EQ(result.value(), v) << "Key: " << k;
    }
}

TEST_F(SSTableReaderTest, IteratorSeekToFirst) {
    std::map<std::string, std::string> data = {
        {"a", "1"},
        {"b", "2"},
        {"c", "3"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto iter = reader.NewIterator();
    
    iter->SeekToFirst();
    
    EXPECT_TRUE(iter->Valid());
    EXPECT_EQ(iter->Key(), "a");
    EXPECT_EQ(iter->GetValue(), "1");
}

TEST_F(SSTableReaderTest, IteratorSequentialScan) {
    std::map<std::string, std::string> data = {
        {"a", "1"},
        {"b", "2"},
        {"c", "3"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto iter = reader.NewIterator();
    
    iter->SeekToFirst();
    
    int count = 0;
    while (iter->Valid()) {
        count++;
        iter->Next();
    }
    
    EXPECT_EQ(count, 3);
}

TEST_F(SSTableReaderTest, IteratorSeekMiddle) {
    std::map<std::string, std::string> data = {
        {"a", "1"},
        {"b", "2"},
        {"c", "3"},
        {"d", "4"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto iter = reader.NewIterator();
    
    iter->Seek("c");
    
    EXPECT_TRUE(iter->Valid());
    EXPECT_EQ(iter->Key(), "c");
    EXPECT_EQ(iter->GetValue(), "3");
}

TEST_F(SSTableReaderTest, IteratorSeekNonExistent) {
    std::map<std::string, std::string> data = {
        {"a", "1"},
        {"c", "3"},
        {"e", "5"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto iter = reader.NewIterator();
    
    iter->Seek("d");
    
    EXPECT_TRUE(iter->Valid());
    EXPECT_EQ(iter->Key(), "e");
    EXPECT_EQ(iter->GetValue(), "5");
}

TEST_F(SSTableReaderTest, IteratorSeekPastEnd) {
    std::map<std::string, std::string> data = {
        {"a", "1"},
        {"b", "2"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto iter = reader.NewIterator();
    
    iter->Seek("z");
    
    EXPECT_FALSE(iter->Valid());
}

TEST_F(SSTableReaderTest, LargeDataset) {
    std::map<std::string, std::string> data;
    for (int i = 0; i < 1000; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = "value_" + std::to_string(i);
        data[key] = value;
    }
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    
    auto result = reader.Get("key_500");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value_500");
    
    result = reader.Get("key_999");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value_999");
    
    result = reader.Get("key_0");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "value_0");
}

TEST_F(SSTableReaderTest, MultipleBlocksIterator) {
    std::map<std::string, std::string> data;
    for (int i = 0; i < 500; ++i) {
        std::string key = "key_" + std::to_string(i);
        std::string value = std::string(100, 'x');
        data[key] = value;
    }
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    auto iter = reader.NewIterator();
    
    iter->SeekToFirst();
    
    int count = 0;
    std::string prev_key;
    while (iter->Valid()) {
        if (!prev_key.empty()) {
            EXPECT_GT(iter->Key(), prev_key) << "Keys should be in ascending order";
        }
        prev_key = iter->Key();
        count++;
        iter->Next();
    }
    
    EXPECT_EQ(count, 500);
}

TEST_F(SSTableReaderTest, IndexEntriesLoaded) {
    std::map<std::string, std::string> data = {
        {"a", "1"},
        {"b", "2"},
        {"c", "3"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    const auto& entries = reader.GetIndexEntries();
    
    EXPECT_FALSE(entries.empty());
    EXPECT_EQ(entries.back().last_key, "c");
}

TEST_F(SSTableReaderTest, EmptyValue) {
    std::map<std::string, std::string> data = {
        {"empty_key", ""},
        {"normal_key", "normal_value"}
    };
    BuildSSTable(data);

    SSTableReader reader(kTestFile);
    
    auto result = reader.Get("empty_key");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "");
    
    result = reader.Get("normal_key");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), "normal_value");
}
