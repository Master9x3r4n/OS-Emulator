#include <memory/MemoryManager.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

MemoryManager::MemoryManager(uint64_t maxOverallMem, uint64_t memPerFrame,
                             std::shared_ptr<BackingStore> backingStore)
    : maxOverallMem_(maxOverallMem),
      memPerFrame_(memPerFrame),
      totalFrames_(memPerFrame == 0 ? 0 : (maxOverallMem / memPerFrame)),
      backingStore_(std::move(backingStore)),
      frames_(totalFrames_),
      lruIterators_(totalFrames_),
      inLru_(totalFrames_, false)
{
    if (!backingStore_)
    {
        backingStore_ = std::make_shared<BackingStore>();
    }

    for (uint32_t i = 0; i < totalFrames_; ++i)
    {
        frames_[i].frameNumber = i;
        frames_[i].occupied = false;
        frames_[i].data.assign(memPerFrame_, 0);
        freeFrames_.push_back(i);
    }
}

MemoryManager::MemoryManager(const Config& config,
                             std::shared_ptr<BackingStore> backingStore)
    : MemoryManager(config.maxOverallMem, config.memPerFrame, std::move(backingStore))
{
}

std::string MemoryManager::serializePage(const std::vector<uint8_t>& data)
{
    static const char hexChars[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t b : data)
    {
        out.push_back(hexChars[(b >> 4) & 0x0F]);
        out.push_back(hexChars[b & 0x0F]);
    }
    return out;
}

std::vector<uint8_t> MemoryManager::deserializePage(const std::string& content, size_t pageSize)
{
    std::vector<uint8_t> data(pageSize, 0);
    if (content.empty())
    {
        return data;
    }

    // Continuous 2-char hex encoding
    if (content.size() == pageSize * 2)
    {
        bool allHex = true;
        for (char c : content)
        {
            if (!std::isxdigit(static_cast<unsigned char>(c)))
            {
                allHex = false;
                break;
            }
        }
        if (allHex)
        {
            for (size_t i = 0; i < pageSize; ++i)
            {
                const std::string byteStr = content.substr(i * 2, 2);
                data[i] = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
            }
            return data;
        }
    }

    // Space-separated tokens fallback
    std::stringstream ss(content);
    std::string token;
    std::vector<std::string> tokens;
    while (ss >> token)
    {
        tokens.push_back(token);
    }
    if (tokens.size() == pageSize)
    {
        bool valid = true;
        std::vector<uint8_t> parsed(pageSize, 0);
        for (size_t i = 0; i < pageSize; ++i)
        {
            try
            {
                size_t idx = 0;
                unsigned long v = std::stoul(tokens[i], &idx, 16);
                if (idx != tokens[i].size() || v > 255)
                {
                    valid = false;
                    break;
                }
                parsed[i] = static_cast<uint8_t>(v);
            }
            catch (...)
            {
                valid = false;
                break;
            }
        }
        if (valid)
        {
            return parsed;
        }
    }

    // Raw bytes fallback
    const size_t copyLen = std::min(content.size(), pageSize);
    for (size_t i = 0; i < copyLen; ++i)
    {
        data[i] = static_cast<uint8_t>(content[i]);
    }
    return data;
}

bool MemoryManager::allocateProcess(const std::string& processName, uint64_t memorySize)
{
    if (processName.empty())
    {
        return false;
    }

    std::lock_guard<std::mutex> lk(mtx_);

    // If already exists, release old frames and clear stale backing store data first
    if (pageTables_.find(processName) != pageTables_.end())
    {
        auto ptIt = pageTables_.find(processName);
        auto& pt = ptIt->second;
        for (size_t i = 0; i < pt->numPages(); ++i)
        {
            auto& entry = pt->getEntry(i);
            if (entry.valid && entry.frameNumber >= 0)
            {
                const uint32_t f = static_cast<uint32_t>(entry.frameNumber);
                removeFrameFromLruLocked(f);

                frames_[f].occupied = false;
                frames_[f].processName.clear();
                frames_[f].pageNumber = 0;
                frames_[f].data.assign(memPerFrame_, 0);
                freeFrames_.push_back(f);

                entry.valid = false;
                entry.frameNumber = -1;
            }
        }

        if (backingStore_)
        {
            backingStore_->discardProcess(processName);
        }

        pageTables_.erase(ptIt);
        processMemorySize_.erase(processName);
    }

    const size_t pagesNeeded = PageTable::calculatePagesNeeded(memorySize, memPerFrame_);

    auto pt = std::make_shared<PageTable>(pagesNeeded);
    pageTables_[processName] = pt;
    processMemorySize_[processName] = memorySize;
    return true;
}

bool MemoryManager::releaseProcess(const std::string& processName)
{
    std::lock_guard<std::mutex> lk(mtx_);
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end())
    {
        return false;
    }

    auto& pt = ptIt->second;
    for (size_t i = 0; i < pt->numPages(); ++i)
    {
        auto& entry = pt->getEntry(i);
        if (entry.valid && entry.frameNumber >= 0)
        {
            const uint32_t f = static_cast<uint32_t>(entry.frameNumber);
            removeFrameFromLruLocked(f);

            frames_[f].occupied = false;
            frames_[f].processName.clear();
            frames_[f].pageNumber = 0;
            frames_[f].data.assign(memPerFrame_, 0);
            freeFrames_.push_back(f);

            entry.valid = false;
            entry.frameNumber = -1;
        }
    }

    if (backingStore_)
    {
        backingStore_->discardProcess(processName);
    }

    pageTables_.erase(ptIt);
    processMemorySize_.erase(processName);
    return true;
}

void MemoryManager::touchFrameLocked(uint32_t frameIndex)
{
    if (inLru_[frameIndex])
    {
        lruList_.erase(lruIterators_[frameIndex]);
    }
    lruList_.push_front(frameIndex);
    lruIterators_[frameIndex] = lruList_.begin();
    inLru_[frameIndex] = true;
}

void MemoryManager::removeFrameFromLruLocked(uint32_t frameIndex)
{
    if (inLru_[frameIndex])
    {
        lruList_.erase(lruIterators_[frameIndex]);
        inLru_[frameIndex] = false;
    }
}

uint32_t MemoryManager::evictLruLocked()
{
    if (lruList_.empty())
    {
        return 0;
    }

    const uint32_t victim = lruList_.back();
    lruList_.pop_back();
    inLru_[victim] = false;

    const std::string victimProc = frames_[victim].processName;
    const uint32_t victimPage = frames_[victim].pageNumber;

    if (backingStore_)
    {
        const std::string serialized = serializePage(frames_[victim].data);
        backingStore_->storePage(victimProc, victimPage, serialized);
    }

    auto ptIt = pageTables_.find(victimProc);
    if (ptIt != pageTables_.end())
    {
        if (victimPage < ptIt->second->numPages())
        {
            ptIt->second->getEntry(victimPage).valid = false;
            ptIt->second->getEntry(victimPage).frameNumber = -1;
        }
    }

    ++pagedOutCounter_;
    return victim;
}

MemoryManager::AccessResult MemoryManager::accessLocked(const std::string& processName, uint64_t address)
{
    if (totalFrames_ == 0 || memPerFrame_ == 0)
    {
        return AccessResult::InvalidAddress;
    }

    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end())
    {
        return AccessResult::InvalidAddress;
    }

    auto szIt = processMemorySize_.find(processName);
    const uint64_t memorySize = (szIt != processMemorySize_.end()) ? szIt->second : 0;
    const uint64_t maxAccessible = std::max<uint64_t>(memorySize, ptIt->second->numPages() * memPerFrame_);

    if (address >= maxAccessible)
    {
        return AccessResult::InvalidAddress;
    }

    const uint32_t pageNumber = static_cast<uint32_t>(address / memPerFrame_);
    return accessPageLocked(processName, pageNumber);
}

MemoryManager::AccessResult MemoryManager::accessPageLocked(const std::string& processName, uint32_t pageNumber)
{
    if (totalFrames_ == 0 || memPerFrame_ == 0)
    {
        return AccessResult::InvalidAddress;
    }

    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end())
    {
        return AccessResult::InvalidAddress;
    }

    auto& pt = ptIt->second;
    if (pageNumber >= pt->numPages())
    {
        return AccessResult::InvalidAddress;
    }

    auto& entry = pt->getEntry(pageNumber);
    if (entry.valid && entry.frameNumber >= 0)
    {
        touchFrameLocked(static_cast<uint32_t>(entry.frameNumber));
        return AccessResult::Hit;
    }

    // Page Fault: obtain a free frame or evict LRU
    uint32_t frameIndex = 0;
    if (!freeFrames_.empty())
    {
        frameIndex = freeFrames_.front();
        freeFrames_.pop_front();
    }
    else
    {
        frameIndex = evictLruLocked();
    }

    // Load from BackingStore or zero-fill
    std::string content;
    bool found = false;
    if (backingStore_)
    {
        found = backingStore_->loadPage(processName, pageNumber, content);
    }

    if (found)
    {
        frames_[frameIndex].data = deserializePage(content, memPerFrame_);
    }
    else
    {
        frames_[frameIndex].data.assign(memPerFrame_, 0);
    }

    frames_[frameIndex].occupied = true;
    frames_[frameIndex].processName = processName;
    frames_[frameIndex].pageNumber = pageNumber;

    entry.valid = true;
    entry.frameNumber = static_cast<int32_t>(frameIndex);

    touchFrameLocked(frameIndex);
    ++pagedInCounter_;
    return AccessResult::PageFault;
}

MemoryManager::AccessResult MemoryManager::access(const std::string& processName, uint64_t address)
{
    std::lock_guard<std::mutex> lk(mtx_);
    return accessLocked(processName, address);
}

MemoryManager::AccessResult MemoryManager::accessPage(const std::string& processName, uint32_t pageNumber)
{
    std::lock_guard<std::mutex> lk(mtx_);
    return accessPageLocked(processName, pageNumber);
}

bool MemoryManager::isResident(const std::string& processName, uint32_t pageNumber) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end() || pageNumber >= ptIt->second->numPages())
    {
        return false;
    }
    return ptIt->second->getEntry(pageNumber).valid;
}

bool MemoryManager::isPageFault(const std::string& processName, uint64_t address) const
{
    if (memPerFrame_ == 0) return true;
    return !isResident(processName, static_cast<uint32_t>(address / memPerFrame_));
}

bool MemoryManager::writeUint16(const std::string& processName, uint64_t address, uint16_t value)
{
    std::lock_guard<std::mutex> lk(mtx_);
    const AccessResult res = accessLocked(processName, address);
    if (res.isInvalid())
    {
        return false;
    }

    const uint32_t pageNumber = static_cast<uint32_t>(address / memPerFrame_);
    const uint32_t offset = static_cast<uint32_t>(address % memPerFrame_);
    if (offset + sizeof(uint16_t) > memPerFrame_)
    {
        return false;
    }

    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end())
    {
        return false;
    }

    const int32_t frameIdx = ptIt->second->getEntry(pageNumber).frameNumber;
    if (frameIdx < 0 || static_cast<size_t>(frameIdx) >= frames_.size())
    {
        return false;
    }

    frames_[frameIdx].data[offset] = static_cast<uint8_t>(value & 0xFF);
    frames_[frameIdx].data[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    touchFrameLocked(static_cast<uint32_t>(frameIdx));
    return true;
}

bool MemoryManager::readUint16(const std::string& processName, uint64_t address, uint16_t& outValue)
{
    std::lock_guard<std::mutex> lk(mtx_);
    const AccessResult res = accessLocked(processName, address);
    if (res.isInvalid())
    {
        return false;
    }

    const uint32_t pageNumber = static_cast<uint32_t>(address / memPerFrame_);
    const uint32_t offset = static_cast<uint32_t>(address % memPerFrame_);
    if (offset + sizeof(uint16_t) > memPerFrame_)
    {
        return false;
    }

    auto ptIt = pageTables_.find(processName);
    if (ptIt == pageTables_.end())
    {
        return false;
    }

    const int32_t frameIdx = ptIt->second->getEntry(pageNumber).frameNumber;
    if (frameIdx < 0 || static_cast<size_t>(frameIdx) >= frames_.size())
    {
        return false;
    }

    outValue = static_cast<uint16_t>(frames_[frameIdx].data[offset]) |
               (static_cast<uint16_t>(frames_[frameIdx].data[offset + 1]) << 8);
    touchFrameLocked(static_cast<uint32_t>(frameIdx));
    return true;
}

uint16_t MemoryManager::readUint16(const std::string& processName, uint64_t address)
{
    uint16_t val = 0;
    readUint16(processName, address, val);
    return val;
}

uint64_t MemoryManager::pagedIn() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return pagedInCounter_;
}

uint64_t MemoryManager::pagedOut() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return pagedOutCounter_;
}

uint64_t MemoryManager::usedFrames() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return totalFrames_ - freeFrames_.size();
}

uint64_t MemoryManager::freeFrames() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return freeFrames_.size();
}

uint64_t MemoryManager::usedMemory() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return (totalFrames_ - freeFrames_.size()) * memPerFrame_;
}

uint64_t MemoryManager::freeMemory() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return freeFrames_.size() * memPerFrame_;
}

std::shared_ptr<PageTable> MemoryManager::getPageTable(const std::string& processName) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = pageTables_.find(processName);
    if (it != pageTables_.end())
    {
        return it->second;
    }
    return nullptr;
}

bool MemoryManager::hasProcess(const std::string& processName) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return pageTables_.find(processName) != pageTables_.end();
}

MemoryManager::MemoryStats MemoryManager::getStats() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    MemoryStats s;
    s.totalFrames = totalFrames_;
    s.freeFrames = freeFrames_.size();
    s.usedFrames = totalFrames_ - s.freeFrames;
    s.totalMemory = maxOverallMem_;
    s.usedMemory = s.usedFrames * memPerFrame_;
    s.freeMemory = s.freeFrames * memPerFrame_;
    s.pagedIn = pagedInCounter_;
    s.pagedOut = pagedOutCounter_;
    return s;
}

void MemoryManager::getMemoryUsage(uint64_t& outUsed, uint64_t& outFree) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    outUsed = (totalFrames_ - freeFrames_.size()) * memPerFrame_;
    outFree = freeFrames_.size() * memPerFrame_;
}

std::pair<uint64_t, uint64_t> MemoryManager::getMemoryUsage() const
{
    uint64_t used = 0;
    uint64_t free = 0;
    getMemoryUsage(used, free);
    return {used, free};
}