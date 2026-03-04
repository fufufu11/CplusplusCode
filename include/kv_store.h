#pragma once
#define _CRT_SECURE_NO_WARNINGS

#include "skiplist.h"
#include "wal_record.h"
#include "sstable_builder.h"
#include "sstable_reader.h"
#include "value_type.h"
#include "compaction_merger.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

struct CompactionConfig {
    size_t l0_trigger_count = 4;
};

class KVStore {
public:
    explicit KVStore(const std::string& dir, const CompactionConfig& config = CompactionConfig{})
        : data_dir_(dir)
        , memtable_(6)
        , next_sstable_id_(1)
        , memtable_size_(0)
        , config_(config) {

        if (!std::filesystem::exists(data_dir_)) {
            std::filesystem::create_directories(data_dir_);
            std::cout << "[KVStore] Created data directory: "
                      << std::filesystem::absolute(data_dir_) << std::endl;
        }

        wal_path_ = data_dir_ / "wal.log";

        LoadExistingSSTables();

        if (std::filesystem::exists(wal_path_) && std::filesystem::file_size(wal_path_) > 0) {
            replay_wal();
        } else {
            std::cout << "[KVStore] WAL initialized (New)." << std::endl;
        }

        wal_file_ = fopen(wal_path_.string().c_str(), "ab");
        if (!wal_file_) {
            throw std::runtime_error("Failed to open WAL file: " + wal_path_.string());
        }
    }

    ~KVStore() {
        if (wal_file_) {
            std::fclose(wal_file_);
            wal_file_ = nullptr;
        }
    }

    void put(int key, const std::string& value) {
        LogRecord record{LogType::kPut, FormatKey(key), Value::normal(value)};
        append_wal(record);

        memtable_.insert(key, Value::normal(value));
        memtable_size_ += EstimateEntrySize(key, value);

        if (memtable_size_ >= kMemTableThreshold) {
            Flush();
        }
    }

    std::optional<std::string> get(int key) {
        auto result = memtable_.search(key);
        if (result.has_value()) {
            if (result->is_tombstone()) {
                return std::nullopt;
            }
            return result->data;
        }

        std::string key_str = FormatKey(key);
        for (auto it = sstable_readers_.rbegin(); it != sstable_readers_.rend(); ++it) {
            auto sstable_result = (*it)->Get(key_str);
            if (sstable_result.has_value()) {
                if (sstable_result->is_tombstone()) {
                    return std::nullopt;
                }
                return sstable_result->data;
            }
        }

        return std::nullopt;
    }

    bool del(int key) {
        LogRecord record{LogType::kDelete, FormatKey(key), Value::tombstone()};
        append_wal(record);
        memtable_.insert(key, Value::tombstone());
        return true;
    }

    void Flush() {
        if (memtable_.empty()) {
            return;
        }

        std::string sstable_filename = "L0_" + std::to_string(next_sstable_id_++) + ".sst";
        std::filesystem::path sstable_path = data_dir_ / sstable_filename;

        std::cout << "[KVStore] Flushing MemTable to " << sstable_filename << std::endl;

        {
            SSTableBuilder builder(sstable_path.string());

            for (auto it = memtable_.begin(); it != memtable_.end(); ++it) {
                std::string key_str = FormatKey(it.key());
                builder.Add(key_str, it.value());
            }

            builder.Finish();
        }

        auto reader = std::make_unique<SSTableReader>(sstable_path.string());
        sstable_readers_.push_back(std::move(reader));

        memtable_.clear();
        memtable_size_ = 0;

        ClearWAL();

        std::cout << "[KVStore] Flush completed. Total SSTables: " << sstable_readers_.size() << std::endl;

        if (ShouldCompact()) {
            DoCompaction();
        }
    }

    size_t GetSSTableCount() const { return sstable_readers_.size(); }

    size_t GetMemTableSize() const { return memtable_size_; }

    size_t GetL0FileCount() const {
        size_t count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
            std::string filename = entry.path().stem().string();
            if (filename.substr(0, 3) == "L0_" && entry.path().extension() == ".sst") {
                count++;
            }
        }
        return count;
    }

private:
    static constexpr size_t kMemTableThreshold = 2 * 1024 * 1024;

    static std::string FormatKey(int key) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%011d", key);
        return std::string(buf);
    }

    std::filesystem::path data_dir_;
    std::filesystem::path wal_path_;
    SkipList<int, Value> memtable_;
    FILE* wal_file_ = nullptr;

    std::vector<std::unique_ptr<SSTableReader>> sstable_readers_;
    uint64_t next_sstable_id_;
    uint64_t next_l1_id_ = 1;
    size_t memtable_size_;
    CompactionConfig config_;

    bool ShouldCompact() const {
        return GetL0FileCount() >= config_.l0_trigger_count;
    }

    void DoCompaction() {
        if (GetL0FileCount() == 0) {
            return;
        }
        
        std::cout << "[KVStore] Starting L0 -> L1 Compaction..." << std::endl;
        
        std::vector<std::filesystem::path> l0_files;
        for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
            std::string filename = entry.path().stem().string();
            if (filename.substr(0, 3) == "L0_" && entry.path().extension() == ".sst") {
                l0_files.push_back(entry.path());
            }
        }
        
        if (l0_files.empty()) {
            return;
        }
        
        std::sort(l0_files.begin(), l0_files.end());
        
        std::vector<std::unique_ptr<SSTableReader>> readers;
        std::vector<std::unique_ptr<SSTableReader::Iterator>> iterators;
        for (const auto& file_path : l0_files) {
            auto reader = std::make_unique<SSTableReader>(file_path.string());
            iterators.push_back(reader->NewIterator());
            readers.push_back(std::move(reader));
        }
        
        std::reverse(iterators.begin(), iterators.end());
        CompactionMerger merger(std::move(iterators));
        
        std::string temp_filename = "L1_temp_" + std::to_string(next_l1_id_) + ".sst";
        std::filesystem::path temp_path = data_dir_ / temp_filename;
        
        {
            SSTableBuilder builder(temp_path.string());
            
            while (merger.Valid()) {
                builder.Add(merger.Key(), merger.GetValue());
                merger.Next();
            }
            
            builder.Finish();
        }
        
        std::string final_filename = "L1_" + std::to_string(next_l1_id_) + ".sst";
        std::filesystem::path final_path = data_dir_ / final_filename;
        std::filesystem::rename(temp_path, final_path);
        
        sstable_readers_.clear();
        auto new_reader = std::make_unique<SSTableReader>(final_path.string());
        sstable_readers_.push_back(std::move(new_reader));
        
        readers.clear();
        
        for (const auto& l0_file : l0_files) {
            std::filesystem::remove(l0_file);
        }
        
        next_l1_id_++;
        
        std::cout << "[KVStore] Compaction completed. L0 files: 0, L1 files: 1" << std::endl;
    }

    size_t EstimateEntrySize(int, const std::string& value) {
        return sizeof(int) + value.size() + 32;
    }

    void LoadExistingSSTables() {
        if (!std::filesystem::exists(data_dir_)) {
            return;
        }

        std::vector<std::filesystem::path> sst_files;

        for (const auto& entry : std::filesystem::directory_iterator(data_dir_)) {
            if (entry.path().extension() == ".sst") {
                sst_files.push_back(entry.path());
            }
        }

        std::sort(sst_files.begin(), sst_files.end());

        for (const auto& sst_path : sst_files) {
            try {
                auto reader = std::make_unique<SSTableReader>(sst_path.string());
                sstable_readers_.push_back(std::move(reader));

                std::string filename = sst_path.stem().string();
                if (filename.substr(0, 3) == "L0_") {
                    uint64_t id = std::stoull(filename.substr(3));
                    if (id >= next_sstable_id_) {
                        next_sstable_id_ = id + 1;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[KVStore] Warning: Failed to load SSTable " 
                          << sst_path << ": " << e.what() << std::endl;
            }
        }

        std::cout << "[KVStore] Loaded " << sstable_readers_.size() << " existing SSTables." << std::endl;
    }

    void ClearWAL() {
        if (wal_file_) {
            std::fclose(wal_file_);
            wal_file_ = nullptr;
        }

        std::filesystem::remove(wal_path_);

        wal_file_ = fopen(wal_path_.string().c_str(), "ab");
        if (!wal_file_) {
            throw std::runtime_error("Failed to recreate WAL file after flush");
        }
    }

    void replay_wal() {
        FILE* fp = fopen(wal_path_.string().c_str(), "rb");
        if (!fp) {
            return;
        }

        std::cout << "[Recovery] Replaying WAL..." << std::endl;

        while (true) {
            uint32_t stored_checksum;
            uint32_t key_len;
            uint32_t value_len;
            uint8_t type_u8;
            uint8_t value_type_u8;

            if (fread(&stored_checksum, 1, 4, fp) != 4) break;
            if (fread(&key_len, 1, 4, fp) != 4) break;
            if (fread(&value_len, 1, 4, fp) != 4) break;
            if (fread(&type_u8, 1, 1, fp) != 1) break;
            if (fread(&value_type_u8, 1, 1, fp) != 1) break;

            std::string key;
            std::string value;

            if (key_len > 0) {
                key.resize(key_len);
                if (fread(key.data(), 1, key_len, fp) != key_len) break;
            }

            if (value_len > 0) {
                value.resize(value_len);
                if (fread(value.data(), 1, value_len, fp) != value_len) break;
            }

            uint32_t payload_total_len = 4 + 4 + 1 + 1 + key_len + value_len;
            std::vector<char> verify_buffer(payload_total_len);
            char* ptr = verify_buffer.data();

            std::memcpy(ptr, &key_len, 4); ptr += 4;
            std::memcpy(ptr, &value_len, 4); ptr += 4;
            std::memcpy(ptr, &type_u8, 1); ptr += 1;
            std::memcpy(ptr, &value_type_u8, 1); ptr += 1;

            if (key_len > 0) {
                std::memcpy(ptr, key.data(), key_len); ptr += key_len;
            }
            if (value_len > 0) {
                std::memcpy(ptr, value.data(), value_len);
            }

            uint32_t calculated_crc = crc32(verify_buffer.data(), payload_total_len);
            if (stored_checksum != calculated_crc) {
                std::cerr << "[Recovery] Checksum mismatch! Stopping." << std::endl;
                break;
            }

            LogType type = static_cast<LogType>(type_u8);
            ValueType value_type = static_cast<ValueType>(value_type_u8);
            try {
                int key_int = std::stoi(key);

                if (type == LogType::kPut && value_type == ValueType::NORMAL) {
                    memtable_.insert(key_int, Value::normal(value));
                    memtable_size_ += EstimateEntrySize(key_int, value);
                } else if (type == LogType::kDelete || value_type == ValueType::TOMBSTONE) {
                    memtable_.insert(key_int, Value::tombstone());
                }
            } catch (...) {
                std::cerr << "[Recovery] Failed to parse key: " << key << ". Skipping." << std::endl;
            }
        }
        fclose(fp);
        std::cout << "[Recovery] WAL replay finished." << std::endl;
    }

    void append_wal(const LogRecord& record) {
        std::string data = encode_log_record(record);

        if (std::fwrite(data.data(), 1, data.size(), wal_file_) != data.size()) {
            throw std::runtime_error("Failed to write to WAL file");
        }

        if (std::fflush(wal_file_) != 0) {
            throw std::runtime_error("WAL flush failed");
        }

#ifdef _WIN32
        if (_commit(_fileno(wal_file_)) != 0) {
            throw std::runtime_error("WAL commit failed");
        }
#else
        if (fsync(fileno(wal_file_)) != 0) {
            throw std::runtime_error("WAL sync (fsync) failed");
        }
#endif
    }
};
