#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace raft {

enum class CommandType : uint8_t {
    kPut,
    kDelete
};

struct Command {
    CommandType type;
    std::string key;
    std::string value;

    Command() = default;

    Command(CommandType t, std::string k, std::string v)
        : type(t)
        , key(std::move(k))
        , value(std::move(v)) {}
};

struct LogEntry {
    uint64_t term;
    uint64_t index;
    Command command;

    LogEntry() = default;

    LogEntry(uint64_t t, uint64_t i, Command cmd)
        : term(t)
        , index(i)
        , command(std::move(cmd)) {}
};

class LogManager {
public:
    LogManager() {
        log_.emplace_back();
    }

    ~LogManager() = default;

    LogManager(const LogManager&) = delete;
    LogManager& operator=(const LogManager&) = delete;

    LogManager(LogManager&&) = delete;
    LogManager& operator=(LogManager&&) = delete;

    void append(const LogEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_.push_back(entry);
    }

    void append(LogEntry&& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        log_.push_back(std::move(entry));
    }

    [[nodiscard]] std::optional<LogEntry> get(uint64_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index == 0 || index >= log_.size()) {
            return std::nullopt;
        }
        return log_[index];
    }

    [[nodiscard]] uint64_t last_log_index() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_.size() <= 1) {
            return 0;
        }
        return log_.back().index;
    }

    [[nodiscard]] uint64_t last_log_term() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (log_.size() <= 1) {
            return 0;
        }
        return log_.back().term;
    }

    void truncate_from(uint64_t index) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index == 0 || index >= log_.size()) {
            return;
        }
        log_.resize(index);
    }

    [[nodiscard]] std::vector<LogEntry> get_entries_from(uint64_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<LogEntry> result;
        if (index == 0 || index >= log_.size()) {
            return result;
        }
        result.reserve(log_.size() - index);
        for (size_t i = index; i < log_.size(); ++i) {
            result.push_back(log_[i]);
        }
        return result;
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return log_.size() > 1 ? log_.size() - 1 : 0;
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return log_.size() <= 1;
    }

private:
    mutable std::mutex mutex_;
    std::vector<LogEntry> log_;
};

}
