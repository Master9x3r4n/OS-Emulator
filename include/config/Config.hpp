#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <stdexcept>

class Config {
public:
    uint8_t numCpu = 4;
    std::string scheduler = "rr";
    uint32_t quantumCycles = 5;
    uint32_t batchProcessFreq = 1;
    uint32_t minIns = 1000;
    uint32_t maxIns = 2000;
    uint32_t delayPerExec = 0;

    Config() = default;
    bool initialize(const std::string& filePath = "config/config.txt");

private:
    static std::string trim(const std::string& s);
    static bool splitKV(const std::string& line, std::string& key, std::string& value);
    static std::string stripQuotes(const std::string& s);
    bool parseEntry(const std::string& key, const std::string& value, int lineNum);
    bool validateCrossFields() const;
};
