/**
 * This module contains Thread-safe process log buffer logic
 * 
 * This code was only modularized after ya'll made the process.cpp a god class ;-;
 * - Stephy
*/

#include <process/ProcessLogBuffer.hpp>

#include <utils/Time.hpp>

ProcessLogBuffer::ProcessLogBuffer(size_t maxCapacity)
    : maxCapacity_(maxCapacity)
{
}

std::string ProcessLogBuffer::formatLogLine(
    const std::string& procName, int core, const std::string& message,
    std::chrono::system_clock::time_point tp)
{
    const std::string coreStr = (core >= 0) ? std::to_string(core) : "N/A";
    std::string text = message.empty() ? ("Hello world from " + procName + "!") : message;
    if (text.empty() || text.front() != '"') {
        text = "\"" + text + "\"";
    }
    return procName + " (" + formatTimestamp(tp) + ") Core: " + coreStr + " " + text;
}

void ProcessLogBuffer::append(const std::string& line)
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (maxCapacity_ > 0)
    {
        while (buffer_.size() >= maxCapacity_)
        {
            buffer_.pop_front();
        }
        buffer_.push_back(line);
    }
}

std::vector<std::string> ProcessLogBuffer::getLogs() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return std::vector<std::string>(buffer_.begin(), buffer_.end());
}

size_t ProcessLogBuffer::size() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return buffer_.size();
}

void ProcessLogBuffer::clear()
{
    std::lock_guard<std::mutex> lk(mtx_);
    buffer_.clear();
}

size_t ProcessLogBuffer::capacity() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return maxCapacity_;
}

void ProcessLogBuffer::setCapacity(size_t cap)
{
    std::lock_guard<std::mutex> lk(mtx_);
    maxCapacity_ = cap;
    while (buffer_.size() > maxCapacity_)
    {
        buffer_.pop_front();
    }
}