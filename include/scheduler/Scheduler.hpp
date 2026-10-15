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

// Multi-threaded CPU scheduler: one scheduler thread plus one worker thread
// per core. The scheduler thread acts as the tick clock. Every tick it
//   1. dispatches ready processes onto idle cores,
//   2. waits one wall-clock tick duration,
//   3. releases all workers to run exactly one tick of work,
//   4. waits for every worker to finish the tick, then
//   5. retires finished processes and (round robin) preempts expired quanta.
// Workers never touch stdout; only the caller's thread prints.
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

    // A point-in-time view used by "screen -ls" and "report-util".
    struct Snapshot {
        uint64_t cpuCycles = 0;
        uint32_t totalCores = 0;
        uint32_t coresUsed = 0;
        double utilization = 0.0; // percent
        std::vector<ProcessInfo> running;  // ordered by core
        std::vector<ProcessInfo> finished; // in order of completion
    };

    Scheduler(const Config& config, std::string logDir,
              std::chrono::milliseconds tickDuration = SchedulerSettings::kTickDuration);
    ~Scheduler();

    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;

    // Spawns the scheduler and worker threads. Returns false on failure.
    bool start();
    // Signals shutdown and joins every thread. Safe to call more than once.
    void stop();

    // Creates a process and puts it at the back of the ready queue.
    // Returns nullptr if a process with that name already exists.
    std::shared_ptr<Process> createProcess(const std::string& name,
                                           uint32_t instructionCount);

    Snapshot snapshot() const;

private:
    void schedulerLoop();
    void workerLoop(uint32_t core);
    void dispatchLocked();
    void collectLocked();
    bool nameExistsLocked(const std::string& name) const;
    void joinAll();
    ProcessInfo describeLocked(const Process& p) const;

    // Configuration (immutable after construction, except loggingEnabled_ which
    // is only changed in start() before any thread runs).
    Policy policy_;
    uint32_t numCores_;
    uint64_t quantumCycles_;
    uint64_t delayPerExec_;
    std::chrono::milliseconds tickDuration_;
    std::string logDir_;
    bool loggingEnabled_;

    // Everything below is guarded by mtx_ (quantumUsed_ elements are written by
    // their own worker during a tick, which is safe because the scheduler
    // thread only touches them between ticks).
    mutable std::mutex mtx_;
    std::condition_variable cvTick_;  // scheduler -> workers: a tick begins
    std::condition_variable cvDone_;  // workers -> scheduler: tick finished
    std::condition_variable cvSleep_; // interruptible wait between ticks
    bool running_ = false;
    uint64_t generation_ = 0;
    uint32_t pending_ = 0;
    uint64_t cpuCycles_ = 0;
    uint32_t nextId_ = 1;

    std::deque<std::shared_ptr<Process>> ready_;
    std::vector<std::shared_ptr<Process>> slots_; // one per core
    std::vector<uint64_t> quantumUsed_;
    std::vector<std::shared_ptr<Process>> finished_;

    std::thread schedulerThread_;
    std::vector<std::thread> workers_;
};
