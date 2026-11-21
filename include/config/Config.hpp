#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

class Config {
public:
    static constexpr uint64_t kMaxValue = 4294967296ULL;

    // bounds for memory parameters ([2^6, 2^16] bytes).
    static constexpr uint64_t kMinMemBytes = 64ULL;
    static constexpr uint64_t kMaxMemBytes = 65536ULL;

    uint8_t numCpu = 4;
    std::string scheduler = "rr";
    uint64_t quantumCycles = 5;
    uint64_t batchProcessFreq = 1;
    uint64_t minIns = 1000;
    uint64_t maxIns = 2000;
    uint64_t delayPerExec = 0;

    // MO2 memory parameters
    uint64_t maxOverallMem = 16384;
    uint64_t memPerFrame = 64;
    uint64_t minMemPerProc = 64;
    uint64_t maxMemPerProc = 4096;

    Config() = default;

    bool initialize(const std::string& filePath = "config/config.txt");

    // Total physical frames available to the memory manager.
    uint64_t totalFrames() const {
        return memPerFrame == 0 ? 0 : (maxOverallMem / memPerFrame);
    }

private:
    static std::string trim(const std::string& s);
    static bool splitKV(const std::string& line, std::string& key, std::string& value);
    static std::string stripQuotes(const std::string& s);
    static bool isPowerOfTwo(uint64_t value);

    enum class NumResult { Ok, NotANumber, OutOfRange };
    static NumResult parseUnsigned(const std::string& text, uint64_t min,
                                   uint64_t max, uint64_t& out);

    bool parseEntry(const std::string& key, const std::string& value, int lineNum);
    bool readNumber(const std::string& key, const std::string& value, int lineNum,
                    uint64_t min, uint64_t max, uint64_t& out) const;
    bool readMemorySize(const std::string& key, const std::string& value, int lineNum,
                        uint64_t& out) const;
    bool validateCrossFields() const;
};