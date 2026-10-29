#pragma once

#include <atomic>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

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
    For
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

    // PRINT:
    std::string message;
    std::string printVar;

    // DECLARE:
    std::string declareVar;
    uint16_t declareValue = 0;

    // ADD / SUBTRACT:
    std::string destVar;
    Operand op1;
    Operand op2;

    // SLEEP:
    uint8_t sleepTicks = 0;

    // FOR:
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
};

struct DecodedInstruction {
    InstructionType type = InstructionType::Print;
    std::string printOutput;
    std::string targetVar;
    uint16_t computedValue = 0;
    uint8_t sleepTicks = 0;
    const std::vector<Instruction>* forBody = nullptr;
    uint32_t forRepeats = 0;
};

class Process {
public:
    static constexpr size_t kMaxNameLength = 14;

    // Constructors:
    // 1. With explicit count (backward-compatible)
    Process(uint32_t id, std::string name = "", uint32_t instructionCount = 0);
    // 2. With config instruction bounds [minIns, maxIns]
    Process(uint32_t id, std::string name, uint64_t minIns, uint64_t maxIns);
    // 3. With explicit pre-built instructions
    Process(uint32_t id, std::string name, std::vector<Instruction> instructions);

    ~Process();

    // Process Attributes
    uint32_t id() const { return id_; }
    const std::string& name() const { return name_; }
    const std::string& createdAt() const { return createdAt_; }
    uint32_t totalInstructions() const { return static_cast<uint32_t>(instructions_.size()); }
    uint32_t executedInstructions() const { return executed_.load(); }
    uint32_t currLine() const { return executed_.load(); }
    bool isFinished() const { return state_.load() == ProcessState::Finished || executed_.load() >= totalInstructions(); }
    bool isWaiting() const { return state_.load() == ProcessState::Waiting; }

    ProcessState state() const { return state_.load(); }
    void setState(ProcessState s) { state_.store(s); }
    int core() const { return core_.load(); }

    void markRunning(int core);
    const std::string& startedAt() const { return startedAt_; }

    // Memory / Symbol Table Management (uint16 clamped between [0, 65535])
    void storeVariable(const std::string& var, uint32_t value);
    uint16_t fetchVariable(const std::string& var);
    bool hasVariable(const std::string& var) const;
    std::unordered_map<std::string, uint16_t> getSymbolTableSnapshot() const;

    // Logging & Reporting
    void enableFileLogging(const std::string& dir);
    std::string generateLogLine(int core) const;
    std::vector<std::string> getLogs() const;

    // Sleep Management
    void putToSleep(uint8_t ticks);
    void tickSleep();

    // Instruction Pipeline
    const Instruction& fetchInstruction() const;
    DecodedInstruction decodeInstruction(const Instruction& inst);
    void executeInstruction(const DecodedInstruction& decoded, uint32_t core);

    // Executes one CPU tick of work
    void executeNext(uint32_t core, uint64_t delayPerExec);

    // Random Instruction Generator
    static std::vector<Instruction> generateRandomInstructions(
        uint64_t minIns, uint64_t maxIns, const std::string& procName, uint32_t maxDepth = 3);

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
    void appendLog(const std::string& logLine);

    uint32_t id_;
    std::string name_;
    std::string createdAt_;
    std::string startedAt_;
    std::vector<Instruction> instructions_;

    std::atomic<uint32_t> executed_{0};
    std::atomic<ProcessState> state_{ProcessState::Ready};
    std::atomic<int> core_{-1};
    uint64_t delayRemaining_ = 0;
    uint32_t sleepTicksRemaining_ = 0;

    mutable std::mutex memoryMtx_;
    std::unordered_map<std::string, uint16_t> symbolTable_;

    mutable std::mutex logMtx_;
    std::vector<std::string> logs_;

    std::vector<LoopFrame> loopStack_;

    bool fileLoggingEnabled_ = false;
    std::string logPath_;
    std::ofstream logFile_;
};