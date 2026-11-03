#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Instruction.hpp"
#include "InstructionGenerator.hpp"
#include "ProcessLogBuffer.hpp"
#include "SymbolTable.hpp"

enum class ProcessState {
    Ready,
    Running,
    Waiting,
    Finished
};

class Process {
public:
    static constexpr size_t kMaxNameLength = 14;

    Process(uint32_t id, std::string name = "", uint32_t instructionCount = 0);
    Process(uint32_t id, std::string name, uint64_t minIns, uint64_t maxIns);
    Process(uint32_t id, std::string name, std::vector<Instruction> instructions);
    ~Process();

    // Identity and State
    uint32_t id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& createdAt() const { return createdAt_; }
    const std::string& startedAt() const { return startedAt_; }
    
    ProcessState state() const { return state_.load(); }
    void setState(ProcessState s) { state_.store(s); }
    
    int core() const { return core_.load(); }
    void setCore(int core) { core_.store(core); }
    void markRunning(int core);

    uint32_t totalInstructions() const { return totalInstructions_; }
    uint32_t executedInstructions() const { return executed_.load(); }
    uint32_t currLine() const { return executed_.load(); }
    
    bool isFinished() const { return state_.load() == ProcessState::Finished || executed_.load() >= totalInstructions_; }
    bool isWaiting() const { return state_.load() == ProcessState::Waiting; }

    // Helpers forwarding to InstructionGenerator
    static uint32_t countTotalInstructions(const std::vector<Instruction>& insts, size_t currentDepth = 0) {
        return InstructionGenerator::countTotalInstructions(insts, currentDepth);
    }
    static std::vector<Instruction> generateRandomInstructions(
        uint64_t minIns, uint64_t maxIns, const std::string& procName, uint32_t currentDepth = 0) {
        return InstructionGenerator::generateRandomInstructions(minIns, maxIns, procName, currentDepth);
    }

    // Symbol Table
    SymbolTable& symbolTable() { return symbolTable_; }
    const SymbolTable& symbolTable() const { return symbolTable_; }
    SymbolTable& getSymbolTable() { return symbolTable_; }
    const SymbolTable& getSymbolTable() const { return symbolTable_; }

    void declareVariable(const std::string& var, int64_t value = 0);
    uint16_t readVariable(const std::string& var);
    void writeVariable(const std::string& var, int64_t value);
    void storeVariable(const std::string& var, int64_t value);
    uint16_t fetchVariable(const std::string& var);
    bool hasVariable(const std::string& var) const;
    std::unordered_map<std::string, uint16_t> getSymbolTableSnapshot() const;

    // Logging
    std::string generateLogLine(int core, const std::string& message = "",
                                std::chrono::system_clock::time_point tp = std::chrono::system_clock::now()) const;
    void appendLog(const std::string& logLine);
    void addLog(const std::string& logLine) { appendLog(logLine); }
    void log(uint32_t core) { appendLog(generateLogLine(core)); }
    std::vector<std::string> getLogs() const;
    std::vector<std::string> logs() const { return getLogs(); }
    size_t logCount() const;
    void clearLogs();
    size_t maxLogCapacity() const;
    void setMaxLogCapacity(size_t cap);

    // Sleep Management
    uint32_t sleepTicksRemaining() const { return sleepTicksRemaining_.load(); }
    void putToSleep(uint8_t ticks);
    void tickSleep();

    // Execution Pipeline
    const Instruction* fetch(uint32_t line) const;
    const Instruction* fetchInstruction() const;
    DecodedInstruction decodeInstruction(const Instruction& inst);
    void executeInstruction(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    bool step(uint32_t core);
    void executeNext(uint32_t core, uint64_t delayPerExec);

    // Shell Screen Attach
    bool isAttached() const { return isAttached_.load(); }
    void setAttached(bool attached) { isAttached_.store(attached); }

private:
    struct LoopFrame {
        const std::vector<Instruction>* instructions = nullptr;
        uint32_t totalRepeats = 0;
        uint32_t currentRepeat = 0;
        size_t pc = 0;
    };

    static std::string sanitizeOrGenerateName(uint32_t id, std::string name);
    uint16_t resolveOperand(const Operand& op);
    void advanceInstructionPointer();

    void handlePrint(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    void handleDeclare(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    void handleAdd(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    void handleSubtract(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    void handleSleep(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    void handleFor(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    void handleInvalid(const DecodedInstruction& decoded, const ExecutionContext& ctx);

    uint32_t id_;
    std::string name_;
    std::string createdAt_;
    std::string startedAt_;
    std::vector<Instruction> instructions_;
    uint32_t totalInstructions_ = 0;
    size_t pc_ = 0;

    std::atomic<uint32_t> executed_{0};
    std::atomic<ProcessState> state_{ProcessState::Ready};
    std::atomic<int> core_{-1};
    uint64_t delayRemaining_ = 0;
    std::atomic<uint32_t> sleepTicksRemaining_{0};

    SymbolTable symbolTable_;
    ProcessLogBuffer logBuffer_;
    std::vector<LoopFrame> loopStack_;
    std::atomic<bool> isAttached_{false};
};