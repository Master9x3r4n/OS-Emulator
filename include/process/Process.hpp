#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "SymbolTable.hpp"

class Process;

enum class ProcessState {
    Ready,
    Running,
    Waiting,
    Finished
};

enum class InstructionType {
    Print,
    Declare,
    Add,
    Subtract,
    Sleep,
    For,
    Invalid
};

struct Operand {
    bool isVariable = false;
    std::string varName;
    uint16_t literalValue = 0;

    Operand() = default;

    static Operand fromVariable(std::string name) {
        Operand op;
        op.isVariable = true;
        op.varName = std::move(name);
        return op;
    }

    static Operand fromValue(uint16_t val) {
        Operand op;
        op.isVariable = false;
        op.literalValue = val;
        return op;
    }
};

struct Instruction {
    InstructionType type = InstructionType::Print;

    std::string message;
    std::string printVar;

    std::string declareVar;
    uint16_t declareValue = 0;

    std::string destVar;
    Operand op1;
    Operand op2;

    uint8_t sleepTicks = 0;

    uint32_t repeats = 1;
    std::vector<Instruction> nestedInstructions;

    static Instruction makePrint(std::string msg = "", std::string var = "") {
        Instruction inst;
        inst.type = InstructionType::Print;
        inst.message = std::move(msg);
        inst.printVar = std::move(var);
        return inst;
    }

    static Instruction makeDeclare(std::string var, uint16_t val) {
        Instruction inst;
        inst.type = InstructionType::Declare;
        inst.declareVar = std::move(var);
        inst.declareValue = val;
        return inst;
    }

    static Instruction makeAdd(std::string dest, Operand op1, Operand op2) {
        Instruction inst;
        inst.type = InstructionType::Add;
        inst.destVar = std::move(dest);
        inst.op1 = std::move(op1);
        inst.op2 = std::move(op2);
        return inst;
    }

    static Instruction makeSubtract(std::string dest, Operand op1, Operand op2) {
        Instruction inst;
        inst.type = InstructionType::Subtract;
        inst.destVar = std::move(dest);
        inst.op1 = std::move(op1);
        inst.op2 = std::move(op2);
        return inst;
    }

    static Instruction makeSleep(uint8_t ticks) {
        Instruction inst;
        inst.type = InstructionType::Sleep;
        inst.sleepTicks = ticks;
        return inst;
    }

    static Instruction makeFor(std::vector<Instruction> body, uint32_t repeats) {
        Instruction inst;
        inst.type = InstructionType::For;
        inst.nestedInstructions = std::move(body);
        inst.repeats = repeats;
        return inst;
    }

    static Instruction makeInvalid() {
        Instruction inst;
        inst.type = InstructionType::Invalid;
        return inst;
    }
};

struct DecodedInstruction {
    InstructionType type = InstructionType::Invalid;
    bool isValid = true;
    std::string printOutput;
    std::string targetVar;
    uint16_t computedValue = 0;
    uint8_t sleepTicks = 0;
    const std::vector<Instruction>* forBody = nullptr;
    uint32_t forRepeats = 0;
};

struct ExecutionContext {
    Process& process;
    uint32_t core;
    std::chrono::system_clock::time_point timestamp;
};

class Process {
public:
    static constexpr size_t kMaxNameLength = 14;
    static constexpr size_t kDefaultMaxLogs = 1000;
    static constexpr size_t kMaxLoopDepth = 3;

    Process(uint32_t id, std::string name = "", uint32_t instructionCount = 0);
    Process(uint32_t id, std::string name, uint64_t minIns, uint64_t maxIns);
    Process(uint32_t id, std::string name, std::vector<Instruction> instructions);
    ~Process();

    uint32_t id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& createdAt() const { return createdAt_; }
    uint32_t totalInstructions() const { return totalInstructions_; }
    uint32_t executedInstructions() const { return executed_.load(); }
    uint32_t currLine() const { return executed_.load(); }
    bool isFinished() const { return state_.load() == ProcessState::Finished || executed_.load() >= totalInstructions_; }
    bool isWaiting() const { return state_.load() == ProcessState::Waiting; }

    // Helper to calculate total instruction count expanding loops
    static uint32_t countTotalInstructions(const std::vector<Instruction>& insts, size_t currentDepth = 0);

    ProcessState state() const { return state_.load(); }
    void setState(ProcessState s) { state_.store(s); }
    int core() const { return core_.load(); }
    void markRunning(int core);
    const std::string& startedAt() const { return startedAt_; }

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

    // =========================================================================
    // Logging & In-Memory Buffer
    // =========================================================================

    // Formats: <process_name> (<date-time>) Core: <core> <msg>
    std::string generateLogLine(int core, const std::string& message = "", std::chrono::system_clock::time_point tp = std::chrono::system_clock::now()) const;

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
    void putToSleep(uint8_t ticks);
    void tickSleep();

    // Pipeline Methods
    const Instruction* fetch(uint32_t line) const;
    const Instruction* fetchInstruction() const;
    DecodedInstruction decodeInstruction(const Instruction& inst);
    void executeInstruction(const DecodedInstruction& decoded, const ExecutionContext& ctx);
    bool step(uint32_t core);
    void executeNext(uint32_t core, uint64_t delayPerExec);

    static std::vector<Instruction> generateRandomInstructions(
        uint64_t minIns, uint64_t maxIns, const std::string& procName, uint32_t currentDepth = 0);
    
    // State Accessors
    bool isAttached() const { return isAttached_.load(); }
    void setAttached(bool attached) { isAttached_.store(attached); }

    // sleep ticks getter and core setters
    uint32_t sleepTicksRemaining() const { return sleepTicksRemaining_.load(); }
    void setCore(int core) { core_.store(core); }

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
    size_t pc_ = 0; // top-level instruction pointer
    
    std::atomic<uint32_t> executed_{0};
    std::atomic<ProcessState> state_{ProcessState::Ready};
    std::atomic<int> core_{-1};
    uint64_t delayRemaining_ = 0;
    std::atomic<uint32_t> sleepTicksRemaining_{0};

    SymbolTable symbolTable_;

    mutable std::mutex logMtx_;
    std::deque<std::string> logs_;
    size_t maxLogs_ = kDefaultMaxLogs;

    std::vector<LoopFrame> loopStack_;

    std::atomic<bool> isAttached_{false};
};