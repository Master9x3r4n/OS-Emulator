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
#include <scheduler/SchedulerTypes.hpp>

class Scheduler {
public:
    using Policy = ::Policy;
    using ProcessInfo = ::ProcessInfo;
    using Snapshot = ::Snapshot;

    Scheduler(const Config& config, std::string logDir,
              std::chrono::milliseconds tickDuration = SchedulerSettings::kTickDuration);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    bool start();
    void stop();

    // Generator controls
    bool startGenerator();
    bool stopGenerator();
    bool isGenerating() const;

    // Process creation (thread-safe)
    std::shared_ptr<Process> createProcess(const std::string& name = "",
                                           std::string* error = nullptr,
                                           uint32_t instructionCount = 0);

    std::shared_ptr<Process> createProcess(const std::string& name,
                                           uint32_t instructionCount);

    Snapshot snapshot() const;
    std::shared_ptr<Process> findProcess(const std::string& name) const;

private:
    void schedulerLoop();
    void workerLoop(uint32_t core);
    void dispatchLocked();
    void collectLocked();
    bool nameExistsLocked(const std::string& name) const;
    void joinAll();
    ProcessInfo describeLocked(const Process& p) const;

    std::shared_ptr<Process> createProcessLocked(const std::string& name = "",
                                                std::string* error = nullptr,
                                                uint32_t instructionCount = 0);

    // Configuration
    Policy policy_;
    uint32_t numCores_;
    uint64_t quantumCycles_;
    uint64_t batchProcessFreq_;
    uint64_t delayPerExec_;
    uint64_t minIns_ = 1000;         
    uint64_t maxIns_ = 2000;          
    std::chrono::milliseconds tickDuration_;
    std::string logDir_;

    // Thread synchronization & state
    mutable std::mutex mtx_;
    std::condition_variable cvTick_;
    std::condition_variable cvDone_;
    std::condition_variable cvSleep_;
    bool running_ = false;
    bool generating_ = false;
    uint64_t generation_ = 0;
    uint32_t pending_ = 0;
    uint64_t cpuCycles_ = 0;
    uint64_t batchTicks_ = 0;
    uint32_t nextId_ = 1;
    uint32_t generatedCount_ = 1;

    std::deque<std::shared_ptr<Process>> ready_;
    std::vector<std::shared_ptr<Process>> slots_;
    std::vector<uint64_t> quantumUsed_;
    std::vector<std::shared_ptr<Process>> waiting_;  
    std::vector<std::shared_ptr<Process>> finished_;

    std::thread schedulerThread_;
    std::vector<std::thread> workers_;
};