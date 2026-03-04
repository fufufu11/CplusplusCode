#include <gtest/gtest.h>
#include "sstable_builder.h"
#include "value_type.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

/**
 * @file sstable_builder_test.cpp
 * @brief SSTableBuilder 的单元测试
 *
 * 测试目标：
 * 1. 验证 SSTable 文件能够正确创建
 * 2. 验证文件大小符合预期（至少包含 Footer）
 * 3. 验证 Footer 中的 Magic Number 正确
 * 4. 验证多 Block 场景下文件结构正确
 */

namespace {
    const std::string kTestDir = "./test_data";
    const std::string kTestFile = kTestDir + "/test.sst";
}

/**
 * @brief 测试夹具：每个测试前创建目录，测试后清理
 */
class SSTableBuilderTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::filesystem::create_directories(kTestDir);
    }

    void TearDown() override {
        std::filesystem::remove_all(kTestDir);
    }
};

/**
 * @brief 基础测试：创建空 SSTable 文件
 *
 * 验证点：
 * 1. 文件能够成功创建
 * 2. 文件大小至少为 48 字节（Footer 固定长度）
 */
TEST_F(SSTableBuilderTest, CreatesFileWithFooter) {
    {
        SSTableBuilder builder(kTestFile);
        builder.Finish();
    }

    EXPECT_TRUE(std::filesystem::exists(kTestFile));
    
    uint64_t file_size = std::filesystem::file_size(kTestFile);
    EXPECT_GE(file_size, 48ull) << "File should be at least 48 bytes (Footer size)";
}

/**
 * @brief 基础测试：写入少量 KV 对
 *
 * 验证点：
 * 1. Add() 方法正常工作
 * 2. 文件大小随数据增加而增长
 */
TEST_F(SSTableBuilderTest, WritesSmallData) {
    {
        SSTableBuilder builder(kTestFile);
        builder.Add("key1", Value::normal("value1"));
        builder.Add("key2", Value::normal("value2"));
        builder.Add("key3", Value::normal("value3"));
        builder.Finish();
    }

    EXPECT_TRUE(std::filesystem::exists(kTestFile));
    
    uint64_t file_size = std::filesystem::file_size(kTestFile);
    EXPECT_GT(file_size, 48ull) << "File should be larger than Footer alone";
}

/**
 * @brief 验证测试：Footer 中的 Magic Number 正确
 *
 * 读取文件最后 8 字节，验证是否为正确的 Magic Number。
 */
TEST_F(SSTableBuilderTest, FooterMagicNumberCorrect) {
    {
        SSTableBuilder builder(kTestFile);
        builder.Add("test_key", Value::normal("test_value"));
        builder.Finish();
    }

    std::ifstream file(kTestFile, std::ios::binary);
    ASSERT_TRUE(file.is_open());
    
    uint64_t file_size = std::filesystem::file_size(kTestFile);
    file.seekg(file_size - sizeof(uint64_t));
    
    uint64_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(uint64_t));
    
    EXPECT_EQ(magic, Footer::kTableMagicNumber) 
        << "Magic number should match the expected value";
}

/**
 * @brief 边界测试：不调用 Finish() 时析构函数自动完成
 *
 * 验证 RAII 行为：即使忘记调用 Finish()，析构函数也应该正确处理。
 */
TEST_F(SSTableBuilderTest, AutoFinishOnDestruction) {
    {
        SSTableBuilder builder(kTestFile);
        builder.Add("auto_key", Value::normal("auto_value"));
    }

    EXPECT_TRUE(std::filesystem::exists(kTestFile));
    
    uint64_t file_size = std::filesystem::file_size(kTestFile);
    EXPECT_GE(file_size, 48ull) << "File should have Footer even without explicit Finish()";
}

/**
 * @brief 压力测试：写入足够多的数据触发多个 Block
 *
 * 验证点：
 * 1. 大量数据写入不崩溃
 * 2. 文件大小显著增长
 */
TEST_F(SSTableBuilderTest, WritesMultipleBlocks) {
    {
        SSTableBuilder builder(kTestFile);
        
        for (int i = 0; i < 1000; ++i) {
            std::string key = "key_" + std::to_string(i);
            std::string value = "value_" + std::to_string(i) + "_data";
            builder.Add(key, Value::normal(value));
        }
        builder.Finish();
    }

    EXPECT_TRUE(std::filesystem::exists(kTestFile));
    
    uint64_t file_size = std::filesystem::file_size(kTestFile);
    EXPECT_GT(file_size, 4096ull) << "File should span multiple blocks";
}

/**
 * @brief 验证测试：FileSize() 返回值正确
 */
TEST_F(SSTableBuilderTest, FileSizeAccurate) {
    SSTableBuilder builder(kTestFile);
    builder.Add("a", Value::normal("b"));
    builder.Finish();

    uint64_t reported_size = builder.FileSize();
    uint64_t actual_size = std::filesystem::file_size(kTestFile);
    
    EXPECT_EQ(reported_size, actual_size) << "FileSize() should match actual file size";
}

/**
 * @brief 验证测试：Finished() 状态正确
 */
TEST_F(SSTableBuilderTest, FinishedStateCorrect) {
    SSTableBuilder builder(kTestFile);
    
    EXPECT_FALSE(builder.Finished()) << "Should not be finished initially";
    
    builder.Finish();
    
    EXPECT_TRUE(builder.Finished()) << "Should be finished after Finish()";
}

/**
 * @brief 异常测试：重复调用 Finish() 应抛出异常
 */
TEST_F(SSTableBuilderTest, DoubleFinishThrows) {
    SSTableBuilder builder(kTestFile);
    builder.Add("key", Value::normal("value"));
    builder.Finish();
    
    EXPECT_THROW(builder.Finish(), std::runtime_error) 
        << "Calling Finish() twice should throw";
}
