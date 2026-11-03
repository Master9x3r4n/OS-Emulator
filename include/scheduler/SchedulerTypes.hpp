#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class Policy { FCFS, RoundRobin };

struct ProcessInfo {
    std::string name;
    std::string startedAt;
    int core = -1;
    uint32_t executed = 0;
    uint32_t total = 0;
};

struct Snapshot {
    uint64_t cpuCycles = 0;
    uint32_t totalCores = 0;
    uint32_t coresUsed = 0;
    double utilization = 0.0;
    std::vector<ProcessInfo> running;
    std::vector<ProcessInfo> finished;
};