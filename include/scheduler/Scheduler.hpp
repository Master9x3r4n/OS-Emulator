#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <config/Config.hpp>
#include <process/Process.hpp>
#include <scheduler/SchedulerSettings.hpp>

class Scheduler {
public:
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

    Scheduler(const Config& config,
              std::chrono::milliseconds tickDuration = SchedulerSettings::kTickDuration);
    // Backwards-compatible constructor
    Scheduler(const Config& config, const std::string& logDir,
              std::chrono::milliseconds tickDuration = SchedulerSettings::kTickDuration);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    bool start();
    void stop();

    // Creates a process with default PRINT instructions
    std::shared_ptr<Process> createProcess(const std::string& name,
                                           uint32_t instructionCount);

    // Creates a process with custom instructions
    std::shared_ptr<Process> createProcess(const std::string& name,
                                           std::vector<Instruction> instructions);

    Snapshot snapshot() const;

private:
    void schedulerLoop();
    void workerLoop(uint32_t core);
    void dispatchLocked();
    void collectLocked();
    bool nameExistsLocked(const std::string& name) const;
    void joinAll();
    ProcessInfo describeLocked(const Process& p) const;

    Policy policy_;
    uint32_t numCores_;
    uint64_t quantumCycles_;
    uint64_t delayPerExec_;
    std::chrono::milliseconds tickDuration_;

    mutable std::mutex mtx_;
    std::condition_variable cvTick_;
    std::condition_variable cvDone_;
    std::condition_variable cvSleep_;
    bool running_ = false;
    uint64_t generation_ = 0;
    uint32_t pending_ = 0;
    uint64_t cpuCycles_ = 0;
    uint32_t nextId_ = 1;

    std::deque<std::shared_ptr<Process>> ready_;
    std::vector<std::shared_ptr<Process>> slots_;
    std::vector<uint64_t> quantumUsed_;
    std::vector<std::shared_ptr<Process>> finished_;

    std::thread schedulerThread_;
    std::vector<std::thread> workers_;
};