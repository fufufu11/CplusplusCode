#include <gtest/gtest.h>
#include "wal_record.h"
#include "value_type.h"
#include <vector>

// 验证 CRC32 算法的正确性
TEST(CRC32Test, BasicCalculation) {
    // 标准测试向量：
    // ASCII "123456789" -> 0xCBF43926
    const char* data = "123456789";
    uint32_t expected = 0xCBF43926;
    EXPECT_EQ(crc32(data, 9), expected);

    // 空数据
    EXPECT_EQ(crc32("", 0), 0);
}

// 验证日志记录的编码逻辑
TEST(WALRecordTest, EncodePutRecord) {
    LogRecord record;
    record.type = LogType::kPut;
    record.key = "key";
    record.value = Value::normal("val");

    std::string encoded = encode_log_record(record);

    // 1. 验证总长度
    // Checksum(4) + KeyLen(4) + ValueLen(4) + Type(1) + ValueType(1) + Key(3) + Value(3) = 20
    EXPECT_EQ(encoded.size(), 20);

    // 2. 验证 Header 中的长度字段 (KeyLen=3, ValueLen=3)
    // 布局：Checksum(0-3) | KeyLen(4-7) | ValueLen(8-11) | Type(12) | ValueType(13) | ...
    uint32_t key_len = 0;
    uint32_t val_len = 0;
    memcpy(&key_len, encoded.data() + 4, 4);
    memcpy(&val_len, encoded.data() + 8, 4);

    EXPECT_EQ(key_len, 3);
    EXPECT_EQ(val_len, 3);

    // 3. 验证 Type (kPut=0)
    uint8_t type_val = static_cast<uint8_t>(encoded[12]);
    EXPECT_EQ(type_val, 0);

    // 4. 验证 ValueType (NORMAL=0)
    uint8_t value_type_val = static_cast<uint8_t>(encoded[13]);
    EXPECT_EQ(value_type_val, 0);

    // 5. 验证 Payload
    std::string key_payload(encoded.data() + 14, 3);
    std::string val_payload(encoded.data() + 17, 3);
    EXPECT_EQ(key_payload, "key");
    EXPECT_EQ(val_payload, "val");

    // 6. 验证 Checksum
    // Checksum 是对 encoded[4...end] 的 CRC32
    uint32_t stored_checksum = 0;
    memcpy(&stored_checksum, encoded.data(), 4);
    
    uint32_t calculated_checksum = crc32(encoded.data() + 4, encoded.size() - 4);
    EXPECT_EQ(stored_checksum, calculated_checksum);
}

TEST(WALRecordTest, EncodeDeleteRecord) {
    LogRecord record;
    record.type = LogType::kDelete;
    record.key = "del_key";
    record.value = Value::tombstone();

    std::string encoded = encode_log_record(record);
    
    // Checksum(4) + KeyLen(4) + ValueLen(4) + Type(1) + ValueType(1) + Key(7) + Value(0) = 21
    EXPECT_EQ(encoded.size(), 21);

    // 验证 Type (kDelete=1)
    uint8_t type_val = static_cast<uint8_t>(encoded[12]);
    EXPECT_EQ(type_val, 1);
    
    // 验证 ValueType (TOMBSTONE=1)
    uint8_t value_type_val = static_cast<uint8_t>(encoded[13]);
    EXPECT_EQ(value_type_val, 1);
    
    // 验证 KeyLen
    uint32_t key_len = 0;
    memcpy(&key_len, encoded.data() + 4, 4);
    EXPECT_EQ(key_len, 7);

    // 验证 ValueLen
    uint32_t val_len = 0;
    memcpy(&val_len, encoded.data() + 8, 4);
    EXPECT_EQ(val_len, 0);
    
    // 验证 Checksum
    uint32_t stored_checksum = 0;
    memcpy(&stored_checksum, encoded.data(), 4);
    uint32_t calculated_checksum = crc32(encoded.data() + 4, encoded.size() - 4);
    EXPECT_EQ(stored_checksum, calculated_checksum);
}
