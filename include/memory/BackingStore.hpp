#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class BackingStore {
public:
    struct PageKey {
        std::string processName;
        uint32_t pageNumber = 0;

        bool operator==(const PageKey& other) const {
            return pageNumber == other.pageNumber && processName == other.processName;
        }

        bool operator<(const PageKey& other) const {
            if (processName != other.processName) {
                return processName < other.processName;
            }
            return pageNumber < other.pageNumber;
        }
    };

    static std::string defaultFilePath();

    explicit BackingStore(std::string filePath = defaultFilePath());
    ~BackingStore() = default;

    BackingStore(const BackingStore&) = delete;
    BackingStore& operator=(const BackingStore&) = delete;

    // Operations
    bool storePage(const std::string& processName, uint32_t pageNumber, const std::string& content);

    bool loadPage(const std::string& processName, uint32_t pageNumber, std::string& outContent) const;
    std::optional<std::string> loadPage(const std::string& processName, uint32_t pageNumber) const;

    bool discardProcess(const std::string& processName);
    size_t discardProcessPages(const std::string& processName);

    // Queries
    bool hasPage(const std::string& processName, uint32_t pageNumber) const;
    size_t totalPages() const;
    size_t pageCountForProcess(const std::string& processName) const;
    const std::string& filePath() const { return filePath_; }

    void clear();

private:
    void ensureDirectoryExistsLocked() const;
    void loadFromFileLocked();
    bool syncToFileLocked() const;

    mutable std::mutex mtx_;
    std::string filePath_;
    std::vector<PageKey> order_;
    std::map<PageKey, std::string> pages_;
};