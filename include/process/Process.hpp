#pragma once

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

enum class ProcessState { Ready, Running, Finished };

// A single barebones instruction. Only PRINT exists for now; DECLARE, ADD,
// SUBTRACT, SLEEP and FOR can be added as new Type values later.
struct Instruction {
    enum class Type { Print };
    Type type = Type::Print;
};

// Thread-safety: the instruction-execution members (executeNext and the
// private state it touches) must only be called by the one worker that
// currently owns the process. executedInstructions(), totalInstructions(),
// isFinished() and state() are safe to call from any thread. markRunning(),
// setState() and startedAt() are guarded by the Scheduler's mutex.
class Process {
public:
    Process(uint32_t id, std::string name, uint32_t instructionCount);

    uint32_t id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& createdAt() const { return createdAt_; }
    uint32_t totalInstructions() const { return static_cast<uint32_t>(instructions_.size()); }
    uint32_t executedInstructions() const { return executed_.load(); }
    bool isFinished() const { return executed_.load() >= totalInstructions(); }

    ProcessState state() const { return state_.load(); }
    void setState(ProcessState s) { state_.store(s); }
    int core() const { return core_.load(); }

    // Scheduler-mutex guarded.
    void markRunning(int core);
    const std::string& startedAt() const { return startedAt_; }

    // TEMPORARY (homework): write PRINT output to <dir>/<name>.txt.
    void enableFileLogging(const std::string& dir);

    // Runs one CPU tick of work on `core`: either burns one tick of the
    // busy-wait delay or executes the next instruction. An instruction
    // therefore takes (delayPerExec + 1) ticks, and the process keeps its
    // core while waiting.
    void executeNext(uint32_t core, uint32_t delayPerExec);

private:
    void executePrint(uint32_t core);

    uint32_t id_;
    std::string name_;
    std::string createdAt_;
    std::string startedAt_;
    std::vector<Instruction> instructions_;

    std::atomic<uint32_t> executed_{0};
    std::atomic<ProcessState> state_{ProcessState::Ready};
    std::atomic<int> core_{-1};
    uint32_t delayRemaining_ = 0;

    bool fileLoggingEnabled_ = false;
    std::string logPath_;
    std::ofstream logFile_;
};
