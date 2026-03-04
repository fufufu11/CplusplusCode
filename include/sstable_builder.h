#pragma once

#include "sstable.h"      // BlockHandle, Footer
#include "value_type.h"   // Value, ValueType
#include "wal_record.h"   // crc32 函数

#include <cstdint>
#include <cstdio>
#include <cstring>        // memset, memcpy
#include <stdexcept>      // runtime_error
#include <string>

/**
 * @brief SSTable 文件构造器
 *
 * SSTableBuilder 负责将内存中的有序 KV 对序列化为磁盘上的 SSTable 文件。
 *
 * 核心流程：
 * 1. 用户调用 Add(key, value) 追加 KV 对
 * 2. 当缓冲区达到 4KB 时，自动写入一个 Data Block
 * 3. 用户调用 Finish() 完成构建，写入 Index Block 和 Footer
 *
 * 文件布局：
 * +----------------+----------------+-----+-------------+--------+
 * | Data Block 1   | Data Block 2   | ... | Index Block | Footer |
 * +----------------+----------------+-----+-------------+--------+
 *
 * @note 本类非线程安全，需在单线程环境下使用。
 * @note 调用者必须保证 Add() 传入的 key 是有序的（升序）。
 */
class SSTableBuilder {
public:
    /**
     * @brief 构造函数：打开输出文件并初始化内部状态
     *
     * @param filepath 输出 SSTable 文件的完整路径（如 "./data/L0_001.sst"）
     * @throw std::runtime_error 如果文件无法创建
     */
    explicit SSTableBuilder(const std::string& filepath)
        : file_(nullptr)
        , offset_(0)
        , finished_(false)
        , current_block_handle_{0, 0} {
        file_ = fopen(filepath.c_str(), "wb");
        if (file_ == nullptr) {
            throw std::runtime_error("Failed to create SSTable file: " + filepath);
        }
    }

    /**
     * @brief 析构函数：确保资源正确释放
     *
     * 如果用户忘记调用 Finish()，析构函数会自动完成构建。
     * 这是 RAII 原则的体现：资源获取即初始化，析构即释放。
     */
    ~SSTableBuilder() {
        if (file_) {
            if (!finished_) {
                try { Finish(); } catch (...) {}
            }
            std::fclose(file_);
            file_ = nullptr;
        }
    }

    SSTableBuilder(const SSTableBuilder&) = delete;
    SSTableBuilder& operator=(const SSTableBuilder&) = delete;

    /**
     * @brief 添加一个键值对到 SSTable
     *
     * 将 KV 对编码后追加到当前 Data Block 缓冲区。
     * 当缓冲区大小达到 4KB 时，自动触发 WriteBlock()。
     *
     * Entry 编码格式：
     * +------------+--------------+--------------+-----------+-------------+
     * | KeyLen(4B) | ValueLen(4B) | ValueType(1B)| Key Bytes | Value Bytes |
     * +------------+--------------+--------------+-----------+-------------+
     *
     * @param key 键（必须按升序添加）
     * @param value 值（包含数据和类型）
     */
    void Add(const std::string& key, const Value& value) {
        uint32_t key_len = static_cast<uint32_t>(key.size());
        uint32_t value_len = static_cast<uint32_t>(value.data.size());
        uint8_t type_val = static_cast<uint8_t>(value.type);

        AppendUint32(data_block_buffer_, key_len);
        AppendUint32(data_block_buffer_, value_len);
        data_block_buffer_.push_back(static_cast<char>(type_val));
        data_block_buffer_.append(key);
        data_block_buffer_.append(value.data);

        last_key_ = key;

        if (data_block_buffer_.size() >= kBlockSize) {
            WriteBlock();
        }
    }

    /**
     * @brief 完成 SSTable 构建
     *
     * 执行以下步骤：
     * 1. 如果缓冲区还有数据，写入最后一个 Data Block
     * 2. 写入 Index Block（包含所有 Block 的索引信息）
     * 3. 写入 Footer（固定 48 字节，包含 Index Block 位置和魔数）
     * 4. 关闭文件
     *
     * @throw std::runtime_error 如果写入失败或重复调用
     */
    void Finish() {
        if (finished_) {
            throw std::runtime_error("Finish() called twice");
        }

        if (!data_block_buffer_.empty()) {
            WriteBlock();
        }

        BlockHandle index_handle;
        if (!index_block_buffer_.empty()) {
            uint32_t crc = crc32(index_block_buffer_.data(), index_block_buffer_.size());
            AppendUint32(index_block_buffer_, crc);

            index_handle.offset = offset_;
            index_handle.size = index_block_buffer_.size();

            std::fwrite(index_block_buffer_.data(), 1, index_block_buffer_.size(), file_);
            offset_ += index_block_buffer_.size();
        }

        WriteFooter(index_handle);

        finished_ = true;
        std::fclose(file_);
        file_ = nullptr;
    }

    uint64_t FileSize() const { return offset_; }
    bool Finished() const { return finished_; }

private:
    static constexpr size_t kBlockSize = 4096;
    static constexpr size_t kFooterSize = 48;

    FILE* file_;
    uint64_t offset_;
    bool finished_;

    std::string data_block_buffer_;
    std::string index_block_buffer_;
    std::string last_key_;
    BlockHandle current_block_handle_;

    /**
     * @brief 追加 uint32_t 到缓冲区（小端序）
     *
     * 将一个 4 字节整数拆开，按小端序追加到字符串末尾。
     * 小端序：低字节在前，高字节在后。
     */
    void AppendUint32(std::string& buf, uint32_t val) {
        buf.append(reinterpret_cast<const char*>(&val), sizeof(uint32_t));
    }

    /**
     * @brief 追加 uint64_t 到缓冲区（小端序）
     */
    void AppendUint64(std::string& buf, uint64_t val) {
        buf.append(reinterpret_cast<const char*>(&val), sizeof(uint64_t));
    }

    /**
     * @brief 将当前缓冲区内容写入文件作为一个 Data Block
     *
     * 核心步骤：
     * 1. 计算 CRC32 校验和并追加到缓冲区末尾
     * 2. 将整个 Block 写入文件
     * 3. 记录 BlockHandle（offset, size）
     * 4. 在 Index Block 缓冲区中追加一条索引记录
     * 5. 清空 Data Block 缓冲区
     *
     * Index Entry 格式：
     * +------------+-----------+-------------+-------------+
     * | KeyLen(4B) | Key Bytes | Offset(8B)  | Size(8B)    |
     * +------------+-----------+-------------+-------------+
     */
    void WriteBlock() {
        if (data_block_buffer_.empty()) return;

        uint32_t crc = crc32(data_block_buffer_.data(), data_block_buffer_.size());
        AppendUint32(data_block_buffer_, crc);

        size_t block_size = data_block_buffer_.size();
        if (std::fwrite(data_block_buffer_.data(), 1, block_size, file_) != block_size) {
            throw std::runtime_error("Failed to write Data Block");
        }

        current_block_handle_.offset = offset_;
        current_block_handle_.size = block_size;
        offset_ += block_size;

        AppendUint32(index_block_buffer_, static_cast<uint32_t>(last_key_.size()));
        index_block_buffer_.append(last_key_);
        AppendUint64(index_block_buffer_, current_block_handle_.offset);
        AppendUint64(index_block_buffer_, current_block_handle_.size);

        data_block_buffer_.clear();
    }

    /**
     * @brief 写入 Footer（固定 48 字节）
     *
     * Footer 布局：
     * +------------------------+------------------------+----------------+
     * | metaindex_handle (20B) | index_handle (20B)     | magic (8B)     |
     * +------------------------+------------------------+----------------+
     *
     * 每个 BlockHandle 编码为 20 字节：
     * - offset: 8 字节
     * - size: 8 字节
     * - padding: 4 字节（填充）
     *
     * Magic Number 用于校验文件是否为合法的 SSTable。
     */
    void WriteFooter(const BlockHandle& index_handle) {
        char footer_buf[kFooterSize];
        std::memset(footer_buf, 0, kFooterSize);

        std::memcpy(footer_buf + 20, &index_handle.offset, sizeof(uint64_t));
        std::memcpy(footer_buf + 28, &index_handle.size, sizeof(uint64_t));

        uint64_t magic = Footer::kTableMagicNumber;
        std::memcpy(footer_buf + 40, &magic, sizeof(uint64_t));

        std::fwrite(footer_buf, 1, kFooterSize, file_);
        offset_ += kFooterSize;
    }
};
