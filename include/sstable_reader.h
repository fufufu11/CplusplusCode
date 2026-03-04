#pragma once

#include "sstable.h"
#include "value_type.h"
#include "wal_record.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * @brief SSTable 文件读取器
 *
 * SSTableReader 负责从磁盘读取 SSTable 文件，提供数据查询能力。
 *
 * 核心流程：
 * 1. Open(file_path)：打开文件，读取 Footer，加载 Index Block
 * 2. Get(key)：通过 Index Block 定位 Data Block，查找目标 KV
 * 3. NewIterator()：创建迭代器，支持顺序遍历
 *
 * @note 本类非线程安全，需在单线程环境下使用。
 */
class SSTableReader {
public:
    /**
     * @brief 索引项：记录每个 Data Block 的元信息
     *
     * 在 Index Block 中，每个条目包含：
     * - last_key：该 Data Block 中最大的 Key
     * - handle：该 Data Block 在文件中的位置和大小
     */
    struct IndexEntry {
        std::string last_key;
        BlockHandle handle;
    };

    /**
     * @brief 构造函数：打开并初始化 SSTable Reader
     *
     * @param filepath SSTable 文件的完整路径
     * @throw std::runtime_error 如果文件无法打开或格式无效
     */
    explicit SSTableReader(const std::string& filepath);

    /**
     * @brief 析构函数：关闭文件句柄
     */
    ~SSTableReader();

    SSTableReader(const SSTableReader&) = delete;
    SSTableReader& operator=(const SSTableReader&) = delete;

    /**
     * @brief 查询指定 Key 的 Value
     *
     * 查找流程：
     * 1. 在 Index Block 中二分查找，定位目标 Key 所在的 Data Block
     * 2. 读取该 Data Block 到内存
     * 3. 在 Data Block 内线性查找目标 Key
     *
     * @param key 待查询的键
     * @return std::optional<Value> 如果找到返回 Value（包含数据和类型），否则返回 nullopt
     */
    std::optional<Value> Get(const std::string& key);

    /**
     * @brief 获取 Index Block 中的所有索引项
     *
     * @return const std::vector<IndexEntry>& 索引项列表（只读）
     */
    const std::vector<IndexEntry>& GetIndexEntries() const { return index_entries_; }

    /**
     * @brief 获取文件大小
     */
    uint64_t FileSize() const { return file_size_; }

    /**
     * @brief 前向声明迭代器类
     */
    class Iterator;

    /**
     * @brief 创建迭代器
     *
     * @return std::unique_ptr<Iterator> 迭代器实例
     */
    std::unique_ptr<Iterator> NewIterator();

private:
    /**
     * @brief 读取并验证 Footer
     *
     * @param footer 输出参数，存储解析后的 Footer
     * @return true 读取成功
     * @return false 读取失败（文件太小或 Magic 不匹配）
     */
    bool ReadFooter(Footer& footer);

    /**
     * @brief 读取并解析 Index Block
     *
     * @param index_handle Index Block 的位置信息（从 Footer 获取）
     * @return true 加载成功
     * @return false 加载失败（CRC 校验失败等）
     */
    bool LoadIndexBlock(const BlockHandle& index_handle);

    /**
     * @brief 从文件读取指定位置的数据块
     *
     * @param handle 数据块的位置信息
     * @param data 输出参数，存储读取的数据
     * @return true 读取成功
     * @return false 读取失败
     */
    bool ReadBlock(const BlockHandle& handle, std::string& data);

    /**
     * @brief 在 Data Block 中查找指定 Key
     *
     * @param block_data Data Block 的原始数据（不含 CRC）
     * @param key 待查找的 Key
     * @return std::optional<Value> 找到返回 Value（包含数据和类型），否则返回 nullopt
     */
    std::optional<Value> SearchInBlock(const std::string& block_data, const std::string& key);

    /**
     * @brief 从缓冲区读取 uint32_t（小端序）
     */
    static uint32_t ReadUint32(const char* ptr) {
        uint32_t val;
        std::memcpy(&val, ptr, sizeof(uint32_t));
        return val;
    }

    /**
     * @brief 从缓冲区读取 uint64_t（小端序）
     */
    static uint64_t ReadUint64(const char* ptr) {
        uint64_t val;
        std::memcpy(&val, ptr, sizeof(uint64_t));
        return val;
    }

private:
    FILE* file_;
    std::string filepath_;
    uint64_t file_size_;
    std::vector<IndexEntry> index_entries_;
};

/**
 * @brief SSTable 迭代器
 *
 * 提供顺序遍历 SSTable 中所有 KV 对的能力。
 *
 * 使用方式：
 * @code
 * auto iter = reader->NewIterator();
 * iter->Seek("start_key");
 * while (iter->Valid()) {
 *     Process(iter->Key(), iter->Value());
 *     iter->Next();
 * }
 * @endcode
 */
class SSTableReader::Iterator {
public:
    /**
     * @brief 构造函数
     *
     * @param reader 关联的 SSTableReader 实例
     */
    explicit Iterator(SSTableReader* reader);

    /**
     * @brief 定位到第一个 >= target 的 Key
     *
     * 实现逻辑：
     * 1. 在 Index Block 中二分查找目标 Block
     * 2. 加载该 Block
     * 3. 在 Block 内线性查找目标 Key
     *
     * @param target 目标 Key
     */
    void Seek(const std::string& target);

    /**
     * @brief 定位到 SSTable 的第一个 Key
     */
    void SeekToFirst();

    /**
     * @brief 移动到下一个 KV 对
     *
     * 如果当前 Block 遍历完毕，自动加载下一个 Block。
     */
    void Next();

    /**
     * @brief 检查迭代器是否有效
     *
     * @return true 当前指向有效的 KV 对
     * @return false 已遍历完毕
     */
    bool Valid() const { return valid_; }

    /**
     * @brief 获取当前 Key
     *
     * @note 调用前必须确保 Valid() 为 true
     */
    const std::string& Key() const { return current_key_; }

    /**
     * @brief 获取当前 Value
     *
     * @note 调用前必须确保 Valid() 为 true
     */
    const ::Value& GetValue() const { return current_value_; }

private:
    /**
     * @brief 加载指定索引位置的 Data Block
     *
     * @param block_index 在 index_entries_ 中的索引
     * @return true 加载成功
     * @return false 加载失败
     */
    bool LoadBlock(size_t block_index);

    /**
     * @brief 在当前 Block 中解析下一个 KV 对
     *
     * @return true 解析成功
     * @return false 已到达 Block 末尾
     */
    bool ParseNextEntry();

private:
    SSTableReader* reader_;
    bool valid_;

    size_t current_block_index_;
    std::string current_block_data_;
    size_t current_offset_in_block_;

    std::string current_key_;
    Value current_value_;
};
