#pragma once

#include <cstdint>
#include <string>

enum class ValueType : uint8_t {
    NORMAL = 0,
    TOMBSTONE = 1
};

struct Value {
    std::string data;
    ValueType type = ValueType::NORMAL;
    
    bool is_tombstone() const { return type == ValueType::TOMBSTONE; }
    
    static Value normal(const std::string& d) { return Value{d, ValueType::NORMAL}; }
    static Value tombstone() { return Value{"", ValueType::TOMBSTONE}; }
    
    bool operator==(const Value& other) const {
        return data == other.data && type == other.type;
    }
    
    bool operator==(const std::string& s) const {
        return type == ValueType::NORMAL && data == s;
    }
};
