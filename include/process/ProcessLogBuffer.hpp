#pragma once

#include <chrono>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

class ProcessLogBuffer {
public:
    static constexpr size_t kDefaultMaxCapacity = 1000;

    explicit ProcessLogBuffer(size_t maxCapacity = kDefaultMaxCapacity);

    static std::string formatLogLine(
        const std::string& procName, int core, const std::string& message = "",
        std::chrono::system_clock::time_point tp = std::chrono::system_clock::now());

    void append(const std::string& line);
    std::vector<std::string> getLogs() const;
    size_t size() const;
    void clear();

    size_t capacity() const;
    void setCapacity(size_t cap);

private:
    mutable std::mutex mtx_;
    std::deque<std::string> buffer_;
    size_t maxCapacity_;
};