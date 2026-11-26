#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

struct PageTableEntry {
    bool valid = false;
    int32_t frameNumber = -1;
};

class PageTable {
public:
    PageTable() = default;

    explicit PageTable(size_t numPages)
        : entries_(numPages)
    {
    }

    static size_t calculatePagesNeeded(uint64_t processMemorySize, uint64_t memPerFrame)
    {
        if (memPerFrame == 0) return 1;
        uint64_t pages = (processMemorySize + memPerFrame - 1) / memPerFrame;
        return std::max<size_t>(1, static_cast<size_t>(pages));
    }

    size_t numPages() const { return entries_.size(); }
    size_t size() const { return entries_.size(); }

    const PageTableEntry& getEntry(size_t pageNumber) const { return entries_.at(pageNumber); }
    PageTableEntry& getEntry(size_t pageNumber) { return entries_.at(pageNumber); }

    const PageTableEntry& operator[](size_t pageNumber) const { return entries_[pageNumber]; }
    PageTableEntry& operator[](size_t pageNumber) { return entries_[pageNumber]; }

    const std::vector<PageTableEntry>& entries() const { return entries_; }
    std::vector<PageTableEntry>& entries() { return entries_; }

    void resize(size_t numPages) { entries_.resize(numPages); }
    void clear() { entries_.clear(); }

private:
    std::vector<PageTableEntry> entries_;
};