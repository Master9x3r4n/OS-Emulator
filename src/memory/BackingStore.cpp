#include <memory/BackingStore.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifndef PROJECT_DIR
#define PROJECT_DIR "."
#endif

std::string BackingStore::defaultFilePath()
{
    return std::string(PROJECT_DIR) + "/logs/csopesy-backing-store.txt";
}

BackingStore::BackingStore(std::string filePath)
    : filePath_(std::move(filePath))
{
    std::lock_guard<std::mutex> lk(mtx_);
    ensureDirectoryExistsLocked();
    if (std::filesystem::exists(filePath_))
    {
        loadFromFileLocked();
    }
    else
    {
        std::ofstream out(filePath_);
    }
}

void BackingStore::ensureDirectoryExistsLocked() const
{
    std::filesystem::path p(filePath_);
    if (p.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
}

void BackingStore::loadFromFileLocked()
{
    std::ifstream file(filePath_);
    if (!file.is_open())
    {
        return;
    }

    order_.clear();
    pages_.clear();

    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }

        const size_t firstSpace = line.find(' ');
        if (firstSpace == std::string::npos)
        {
            continue;
        }

        const std::string procName = line.substr(0, firstSpace);
        const size_t secondSpace = line.find(' ', firstSpace + 1);

        std::string pageNumStr;
        std::string content;
        if (secondSpace == std::string::npos)
        {
            pageNumStr = line.substr(firstSpace + 1);
            content = "";
        }
        else
        {
            pageNumStr = line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            content = line.substr(secondSpace + 1);
        }

        if (pageNumStr.empty())
        {
            continue;
        }

        try
        {
            size_t idx = 0;
            const unsigned long val = std::stoul(pageNumStr, &idx);
            if (idx != pageNumStr.size())
            {
                continue;
            }

            PageKey key{procName, static_cast<uint32_t>(val)};
            if (pages_.find(key) == pages_.end())
            {
                order_.push_back(key);
            }
            pages_[key] = std::move(content);
        }
        catch (...)
        {
            continue;
        }
    }
}

bool BackingStore::syncToFileLocked() const
{
    ensureDirectoryExistsLocked();
    std::ofstream file(filePath_, std::ios::out | std::ios::trunc);
    if (!file.is_open())
    {
        return false;
    }

    for (const auto& key : order_)
    {
        auto it = pages_.find(key);
        if (it != pages_.end())
        {
            if (it->second.empty())
            {
                file << key.processName << " " << key.pageNumber << "\n";
            }
            else
            {
                file << key.processName << " " << key.pageNumber << " " << it->second << "\n";
            }
        }
    }

    file.flush();
    file.close();
    return true;
}

bool BackingStore::storePage(const std::string& processName, uint32_t pageNumber, const std::string& content)
{
    if (processName.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lk(mtx_);
    const PageKey key{processName, pageNumber};

    auto it = pages_.find(key);
    if (it != pages_.end())
    {
        it->second = content;
    }
    else
    {
        order_.push_back(key);
        pages_.emplace(key, content);
    }

    return syncToFileLocked();
}

bool BackingStore::loadPage(const std::string& processName, uint32_t pageNumber, std::string& outContent) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    const PageKey key{processName, pageNumber};
    auto it = pages_.find(key);
    if (it == pages_.end())
    {
        return false;
    }
    outContent = it->second;
    return true;
}

std::optional<std::string> BackingStore::loadPage(const std::string& processName, uint32_t pageNumber) const
{
    std::string content;
    if (loadPage(processName, pageNumber, content))
    {
        return content;
    }
    return std::nullopt;
}

bool BackingStore::discardProcess(const std::string& processName)
{
    return discardProcessPages(processName) > 0;
}

size_t BackingStore::discardProcessPages(const std::string& processName)
{
    std::lock_guard<std::mutex> lk(mtx_);
    size_t removedCount = 0;

    auto orderIt = std::remove_if(order_.begin(), order_.end(),
        [&processName, &removedCount](const PageKey& k) {
            if (k.processName == processName) {
                ++removedCount;
                return true;
            }
            return false;
        });
    order_.erase(orderIt, order_.end());

    for (auto it = pages_.begin(); it != pages_.end(); )
    {
        if (it->first.processName == processName)
        {
            it = pages_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    syncToFileLocked();
    return removedCount;
}

bool BackingStore::hasPage(const std::string& processName, uint32_t pageNumber) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return pages_.find(PageKey{processName, pageNumber}) != pages_.end();
}

size_t BackingStore::totalPages() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return pages_.size();
}

size_t BackingStore::pageCountForProcess(const std::string& processName) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    size_t count = 0;
    for (const auto& entry : pages_)
    {
        if (entry.first.processName == processName)
        {
            ++count;
        }
    }
    return count;
}

void BackingStore::clear()
{
    std::lock_guard<std::mutex> lk(mtx_);
    order_.clear();
    pages_.clear();
    syncToFileLocked();
}