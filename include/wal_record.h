#pragma once

#include "value_type.h"

#include <string>
#include <vector>
#include <cstdint>
#include <cstring>


/**
 * @brief 计算 CRC32 校验和（多项式 0xEDB88320，初值 0xFFFFFFFF，结果取反）。
 *
 * @param data 待校验的数据起始地址。
 * @param length 待校验的数据长度（字节数）。
 * @return uint32_t CRC32 校验和。
 */
inline uint32_t crc32(const char* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for(size_t i = 0; i < length; i++) {
        uint8_t byte = static_cast<uint8_t> (data[i]);
        crc = crc ^ byte;
        for(int j = 0; j < 8; j++) {
            uint32_t mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
    }
    return ~crc;
}

/**
 * @brief WAL 日志记录类型。
 *
 * - kPut: 写入/更新键值对
 * - kDelete: 删除键
 */
enum class LogType : uint8_t {
    kPut = 0,
    kDelete = 1
};

/**
 * @brief 一条 WAL 日志记录（逻辑层表示）。
 */
struct LogRecord {
    LogType type;
    std::string key;
    Value value;
};

/**
 * @brief 将一条日志记录编码为二进制字节序列。
 *
 * 编码格式：
 * - Checksum (4B) | KeyLen (4B) | ValueLen (4B) | Type (1B) | ValueType (1B) | Key | Value
 *
 * 其中 Checksum 为对后续字段（从 KeyLen 起直到 Value 结束）的 CRC32：
 * - CRC32( KeyLen | ValueLen | Type | ValueType | Key | Value )
 *
 * @param record 待编码的日志记录。
 * @return std::string 编码后的字节序列（可直接写入 WAL 文件）。
 */
inline std::string encode_log_record(const LogRecord& record) {
    uint32_t key_len = static_cast<uint32_t> (record.key.size());
    uint32_t value_len = static_cast<uint32_t> (record.value.data.size());

    uint32_t total_len = 4 + 4 + 4 + 1 + 1 + key_len + value_len;

    std::string buffer;
    buffer.resize(total_len);
    char* ptr = buffer.data();

    ptr += 4;

    memcpy(ptr, &key_len, sizeof(key_len));
    ptr += sizeof(uint32_t);

    memcpy(ptr, &value_len, sizeof(uint32_t));
    ptr += sizeof(uint32_t);

    uint8_t type_val = static_cast<uint8_t> (record.type);
    memcpy(ptr, &type_val, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    uint8_t value_type_val = static_cast<uint8_t> (record.value.type);
    memcpy(ptr, &value_type_val, sizeof(uint8_t));
    ptr += sizeof(uint8_t);

    if(key_len > 0) {
        memcpy(ptr, record.key.data(), key_len);
        ptr += key_len;
    }

    if(value_len > 0) {
        memcpy(ptr, record.value.data.data(), value_len);
        ptr += value_len;
    }

    uint32_t checksum = crc32(buffer.data() + 4, total_len - 4);
    memcpy(buffer.data(), &checksum, sizeof(uint32_t));

    return buffer;
}
