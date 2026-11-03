#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

class SymbolTable {
public:
    static constexpr uint16_t kMinValue = 0;
    static constexpr uint16_t kMaxValue = 65535;

    SymbolTable() = default;

    SymbolTable(const SymbolTable& other) {
        std::lock_guard<std::mutex> lk(other.mtx_);
        table_ = other.table_;
    }

    SymbolTable& operator=(const SymbolTable& other) {
        if (this != &other) {
            std::scoped_lock lock(mtx_, other.mtx_);
            table_ = other.table_;
        }
        return *this;
    }

    static uint16_t clamp(int64_t value) {
        if (value < static_cast<int64_t>(kMinValue))
            return kMinValue;
        if (value > static_cast<int64_t>(kMaxValue))
            return kMaxValue;
        return static_cast<uint16_t>(value);
    }

    void declare(const std::string& name, int64_t initialValue = 0) {
        std::lock_guard<std::mutex> lk(mtx_);
        table_[name] = clamp(initialValue);
    }

    uint16_t read(const std::string& name) {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = table_.find(name);
        if (it == table_.end()) {
            table_[name] = 0;
            return 0;
        }
        return it->second;
    }

    void write(const std::string& name, int64_t value) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (table_.find(name) == table_.end()) {
            table_[name] = 0;
        }
        table_[name] = clamp(value);
    }

    uint16_t get(const std::string& name) { return read(name); }
    void set(const std::string& name, int64_t value) { write(name, value); }

    bool has(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mtx_);
        return table_.find(name) != table_.end();
    }
    bool contains(const std::string& name) const { return has(name); }

    std::unordered_map<std::string, uint16_t> snapshot() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return table_;
    }
    std::unordered_map<std::string, uint16_t> getAll() const { return snapshot(); }

    size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return table_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        table_.clear();
    }

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, uint16_t> table_;
};