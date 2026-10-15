#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

class Config {
public:
    // Upper bound for the 32-bit style parameters in the spec ("[1, 2^32]").
    // Stored as uint64_t because 2^32 itself does not fit in a uint32_t.
    static constexpr uint64_t kMaxValue = 4294967296ULL;

    uint8_t numCpu = 4;
    std::string scheduler = "rr";
    uint64_t quantumCycles = 5;
    uint64_t batchProcessFreq = 1;
    uint64_t minIns = 1000;
    uint64_t maxIns = 2000;
    uint64_t delayPerExec = 0;

    Config() = default;

    // Loads and validates the config file. On any error the object is left
    // unchanged and false is returned, so a bad file never leaves a
    // half-applied configuration behind.
    bool initialize(const std::string& filePath = "config/config.txt");

private:
    static std::string trim(const std::string& s);
    static bool splitKV(const std::string& line, std::string& key, std::string& value);
    static std::string stripQuotes(const std::string& s);

    enum class NumResult { Ok, NotANumber, OutOfRange };
    // Accepts only plain unsigned decimal digits (no sign, spaces, suffixes).
    static NumResult parseUnsigned(const std::string& text, uint64_t min,
                                   uint64_t max, uint64_t& out);

    bool parseEntry(const std::string& key, const std::string& value, int lineNum);
    bool readNumber(const std::string& key, const std::string& value, int lineNum,
                    uint64_t min, uint64_t max, uint64_t& out) const;
    bool validateCrossFields() const;
};
