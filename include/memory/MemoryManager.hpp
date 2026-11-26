#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <config/Config.hpp>
#include <memory/BackingStore.hpp>
#include <memory/PageTable.hpp>

class MemoryManager {
public:
    struct AccessResult {
        enum Type {
            Hit,
            PageFault,
            InvalidAddress
        };

        Type type;

        AccessResult(Type t = Hit) : type(t) {}

        bool isHit() const { return type == Hit; }
        bool isPageFault() const { return type == PageFault; }
        bool isInvalid() const { return type == InvalidAddress; }

        operator Type() const { return type; }
        bool operator==(Type t) const { return type == t; }
        bool operator!=(Type t) const { return type != t; }
        bool operator==(const AccessResult& o) const { return type == o.type; }
        bool operator!=(const AccessResult& o) const { return type != o.type; }

        friend std::ostream& operator<<(std::ostream& os, const AccessResult& r) {
            switch (r.type) {
                case Hit: os << "Hit"; break;
                case PageFault: os << "PageFault"; break;
                case InvalidAddress: os << "InvalidAddress"; break;
            }
            return os;
        }
    };

    struct Frame {
        uint32_t frameNumber = 0;
        bool occupied = false;
        std::string processName;
        uint32_t pageNumber = 0;
        std::vector<uint8_t> data;
    };

    struct MemoryStats {
        uint64_t totalMemory = 0;
        uint64_t usedMemory = 0;
        uint64_t freeMemory = 0;
        uint64_t totalFrames = 0;
        uint64_t usedFrames = 0;
        uint64_t freeFrames = 0;
        uint64_t pagedIn = 0;
        uint64_t pagedOut = 0;
    };

    // Atomic snapshot methods under a single lock acquisition
    MemoryStats getStats() const;
    void getMemoryUsage(uint64_t& outUsed, uint64_t& outFree) const;
    std::pair<uint64_t, uint64_t> getMemoryUsage() const;

    MemoryManager(uint64_t maxOverallMem, uint64_t memPerFrame,
                  std::shared_ptr<BackingStore> backingStore = nullptr);
    MemoryManager(const Config& config,
                  std::shared_ptr<BackingStore> backingStore = nullptr);

    ~MemoryManager() = default;
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;

    // Process Memory Lifecycle
    bool allocateProcess(const std::string& processName, uint64_t memorySize);
    bool registerProcess(const std::string& processName, uint64_t memorySize) {
        return allocateProcess(processName, memorySize);
    }
    bool releaseProcess(const std::string& processName);
    bool deallocateProcess(const std::string& processName) {
        return releaseProcess(processName);
    }

    // Memory Access
    AccessResult access(const std::string& processName, uint64_t address);
    AccessResult accessAddress(const std::string& processName, uint64_t address) {
        return access(processName, address);
    }
    AccessResult accessPage(const std::string& processName, uint32_t pageNumber);

    bool isResident(const std::string& processName, uint32_t pageNumber) const;
    bool isPageFault(const std::string& processName, uint64_t address) const;

    // Value Reading and Writing (uint16)
    bool writeUint16(const std::string& processName, uint64_t address, uint16_t value);
    bool readUint16(const std::string& processName, uint64_t address, uint16_t& outValue);
    uint16_t readUint16(const std::string& processName, uint64_t address);

    // Aliases
    bool write(const std::string& processName, uint64_t address, uint16_t value) {
        return writeUint16(processName, address, value);
    }
    bool read(const std::string& processName, uint64_t address, uint16_t& outValue) {
        return readUint16(processName, address, outValue);
    }
    uint16_t read(const std::string& processName, uint64_t address) {
        return readUint16(processName, address);
    }

    // Statistics and Counters
    uint64_t pagedIn() const;
    uint64_t numPagedIn() const { return pagedIn(); }

    uint64_t pagedOut() const;
    uint64_t numPagedOut() const { return pagedOut(); }

    uint64_t usedFrames() const;
    uint64_t freeFrames() const;
    uint64_t totalFrames() const { return totalFrames_; }

    uint64_t usedMemory() const;
    uint64_t freeMemory() const;
    uint64_t totalMemory() const { return maxOverallMem_; }
    uint64_t memPerFrame() const { return memPerFrame_; }

    // Page Table Inspection
    std::shared_ptr<PageTable> getPageTable(const std::string& processName) const;
    bool hasProcess(const std::string& processName) const;

    std::shared_ptr<BackingStore> backingStore() const { return backingStore_; }

private:
    AccessResult accessLocked(const std::string& processName, uint64_t address);
    AccessResult accessPageLocked(const std::string& processName, uint32_t pageNumber);

    void touchFrameLocked(uint32_t frameIndex);
    void removeFrameFromLruLocked(uint32_t frameIndex);
    uint32_t evictLruLocked();

    static std::string serializePage(const std::vector<uint8_t>& data);
    static std::vector<uint8_t> deserializePage(const std::string& content, size_t pageSize);

    mutable std::mutex mtx_;
    uint64_t maxOverallMem_;
    uint64_t memPerFrame_;
    uint64_t totalFrames_;

    std::shared_ptr<BackingStore> backingStore_;

    std::vector<Frame> frames_;
    std::deque<uint32_t> freeFrames_;

    std::list<uint32_t> lruList_;
    std::vector<std::list<uint32_t>::iterator> lruIterators_;
    std::vector<bool> inLru_;

    std::unordered_map<std::string, std::shared_ptr<PageTable>> pageTables_;
    std::unordered_map<std::string, uint64_t> processMemorySize_;

    uint64_t pagedInCounter_ = 0;
    uint64_t pagedOutCounter_ = 0;
};