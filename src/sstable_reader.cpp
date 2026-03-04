#include "sstable_reader.h"

#include <algorithm>
#include <filesystem>

SSTableReader::SSTableReader(const std::string& filepath)
    : file_(nullptr)
    , filepath_(filepath)
    , file_size_(0) {
    
    if (!std::filesystem::exists(filepath)) {
        throw std::runtime_error("SSTable file does not exist: " + filepath);
    }

    file_ = fopen(filepath.c_str(), "rb");
    if (!file_) {
        throw std::runtime_error("Failed to open SSTable file: " + filepath);
    }

    file_size_ = std::filesystem::file_size(filepath);

    Footer footer;
    if (!ReadFooter(footer)) {
        fclose(file_);
        file_ = nullptr;
        throw std::runtime_error("Invalid SSTable file: Footer read failed or magic mismatch");
    }

    if (!LoadIndexBlock(footer.index_handle)) {
        fclose(file_);
        file_ = nullptr;
        throw std::runtime_error("Failed to load Index Block");
    }
}

SSTableReader::~SSTableReader() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
}

bool SSTableReader::ReadFooter(Footer& footer) {
    if (file_size_ < Footer::kEncodedLength) {
        return false;
    }

    if (fseek(file_, file_size_ - Footer::kEncodedLength, SEEK_SET) != 0) {
        return false;
    }

    char footer_buf[Footer::kEncodedLength];
    if (fread(footer_buf, 1, Footer::kEncodedLength, file_) != Footer::kEncodedLength) {
        return false;
    }

    uint64_t magic = ReadUint64(footer_buf + 40);
    if (magic != Footer::kTableMagicNumber) {
        return false;
    }

    footer.index_handle.offset = ReadUint64(footer_buf + 20);
    footer.index_handle.size = ReadUint64(footer_buf + 28);

    return true;
}

bool SSTableReader::LoadIndexBlock(const BlockHandle& index_handle) {
    if (index_handle.size == 0 || index_handle.size == 4) {
        return true;
    }

    std::string block_data;
    if (!ReadBlock(index_handle, block_data)) {
        return false;
    }

    size_t offset = 0;
    while (offset < block_data.size()) {
        if (offset + 4 > block_data.size()) break;

        uint32_t key_len = ReadUint32(block_data.data() + offset);
        offset += 4;

        if (offset + key_len > block_data.size()) break;

        std::string last_key(block_data.data() + offset, key_len);
        offset += key_len;

        if (offset + 16 > block_data.size()) break;

        uint64_t block_offset = ReadUint64(block_data.data() + offset);
        offset += 8;
        uint64_t block_size = ReadUint64(block_data.data() + offset);
        offset += 8;

        IndexEntry entry;
        entry.last_key = std::move(last_key);
        entry.handle.offset = block_offset;
        entry.handle.size = block_size;

        index_entries_.push_back(std::move(entry));
    }

    return true;
}

bool SSTableReader::ReadBlock(const BlockHandle& handle, std::string& data) {
    if (handle.offset + handle.size > file_size_) {
        return false;
    }

    if (fseek(file_, handle.offset, SEEK_SET) != 0) {
        return false;
    }

    data.resize(handle.size);
    if (fread(data.data(), 1, handle.size, file_) != handle.size) {
        return false;
    }

    if (handle.size >= 4) {
        uint32_t stored_crc = ReadUint32(data.data() + handle.size - 4);
        uint32_t calculated_crc = crc32(data.data(), handle.size - 4);

        if (stored_crc != calculated_crc) {
            return false;
        }

        data.resize(handle.size - 4);
    }

    return true;
}

std::optional<Value> SSTableReader::Get(const std::string& key) {
    if (index_entries_.empty()) {
        return std::nullopt;
    }

    auto it = std::upper_bound(
        index_entries_.begin(),
        index_entries_.end(),
        key,
        [](const std::string& k, const IndexEntry& entry) {
            return k < entry.last_key;
        }
    );

    size_t block_index;
    if (it == index_entries_.end()) {
        if (key > index_entries_.back().last_key) {
            return std::nullopt;
        }
        block_index = index_entries_.size() - 1;
    } else {
        block_index = std::distance(index_entries_.begin(), it);
    }

    std::string block_data;
    if (!ReadBlock(index_entries_[block_index].handle, block_data)) {
        return std::nullopt;
    }

    return SearchInBlock(block_data, key);
}

std::optional<Value> SSTableReader::SearchInBlock(
    const std::string& block_data, const std::string& key) {
    
    size_t offset = 0;
    while (offset < block_data.size()) {
        if (offset + 8 > block_data.size()) break;

        uint32_t key_len = ReadUint32(block_data.data() + offset);
        offset += 4;
        uint32_t value_len = ReadUint32(block_data.data() + offset);
        offset += 4;

        if (offset + 1 > block_data.size()) break;
        uint8_t type_val = static_cast<uint8_t>(block_data.data()[offset]);
        offset += 1;

        if (offset + key_len > block_data.size()) break;

        std::string entry_key(block_data.data() + offset, key_len);
        offset += key_len;

        if (entry_key == key) {
            if (offset + value_len > block_data.size()) break;
            std::string value_data(block_data.data() + offset, value_len);
            Value value;
            value.data = std::move(value_data);
            value.type = static_cast<ValueType>(type_val);
            return value;
        }

        offset += value_len;
    }

    return std::nullopt;
}

std::unique_ptr<SSTableReader::Iterator> SSTableReader::NewIterator() {
    return std::make_unique<Iterator>(this);
}

SSTableReader::Iterator::Iterator(SSTableReader* reader)
    : reader_(reader)
    , valid_(false)
    , current_block_index_(0)
    , current_offset_in_block_(0) {
}

void SSTableReader::Iterator::Seek(const std::string& target) {
    if (reader_->index_entries_.empty()) {
        valid_ = false;
        return;
    }

    auto it = std::upper_bound(
        reader_->index_entries_.begin(),
        reader_->index_entries_.end(),
        target,
        [](const std::string& k, const IndexEntry& entry) {
            return k < entry.last_key;
        }
    );

    size_t block_index;
    if (it == reader_->index_entries_.end()) {
        block_index = reader_->index_entries_.size() - 1;
    } else {
        block_index = std::distance(reader_->index_entries_.begin(), it);
    }

    if (!LoadBlock(block_index)) {
        valid_ = false;
        return;
    }

    while (ParseNextEntry()) {
        if (current_key_ >= target) {
            valid_ = true;
            return;
        }
    }

    while (current_block_index_ + 1 < reader_->index_entries_.size()) {
        current_block_index_++;
        if (!LoadBlock(current_block_index_)) {
            valid_ = false;
            return;
        }
        if (ParseNextEntry()) {
            valid_ = true;
            return;
        }
    }

    valid_ = false;
}

void SSTableReader::Iterator::SeekToFirst() {
    if (reader_->index_entries_.empty()) {
        valid_ = false;
        return;
    }

    if (!LoadBlock(0)) {
        valid_ = false;
        return;
    }

    valid_ = ParseNextEntry();
}

void SSTableReader::Iterator::Next() {
    if (!valid_) return;

    if (ParseNextEntry()) {
        return;
    }

    while (current_block_index_ + 1 < reader_->index_entries_.size()) {
        current_block_index_++;
        if (!LoadBlock(current_block_index_)) {
            valid_ = false;
            return;
        }
        if (ParseNextEntry()) {
            return;
        }
    }

    valid_ = false;
}

bool SSTableReader::Iterator::LoadBlock(size_t block_index) {
    if (block_index >= reader_->index_entries_.size()) {
        return false;
    }

    if (!reader_->ReadBlock(reader_->index_entries_[block_index].handle, current_block_data_)) {
        return false;
    }

    current_block_index_ = block_index;
    current_offset_in_block_ = 0;
    return true;
}

bool SSTableReader::Iterator::ParseNextEntry() {
    if (current_offset_in_block_ >= current_block_data_.size()) {
        return false;
    }

    size_t offset = current_offset_in_block_;

    if (offset + 8 > current_block_data_.size()) {
        return false;
    }

    uint32_t key_len = reader_->ReadUint32(current_block_data_.data() + offset);
    offset += 4;
    uint32_t value_len = reader_->ReadUint32(current_block_data_.data() + offset);
    offset += 4;

    if (offset + 1 > current_block_data_.size()) {
        return false;
    }
    uint8_t type_val = static_cast<uint8_t>(current_block_data_.data()[offset]);
    offset += 1;

    if (offset + key_len > current_block_data_.size()) {
        return false;
    }

    current_key_.assign(current_block_data_.data() + offset, key_len);
    offset += key_len;

    if (offset + value_len > current_block_data_.size()) {
        return false;
    }

    current_value_.data.assign(current_block_data_.data() + offset, value_len);
    current_value_.type = static_cast<ValueType>(type_val);
    offset += value_len;

    current_offset_in_block_ = offset;
    return true;
}
