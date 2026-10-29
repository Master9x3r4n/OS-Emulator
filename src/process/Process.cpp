#include <process/Process.hpp>

#include <algorithm>
#include <cstdio>
#include <random>
#include <scheduler/SchedulerSettings.hpp>
#include <utils/Time.hpp>

std::string Process::sanitizeOrGenerateName(uint32_t id, std::string name)
{
    if (name.empty())
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "p%02u", id);
        return std::string(buf);
    }
    if (name.length() > kMaxNameLength)
    {
        return name.substr(0, kMaxNameLength);
    }
    return name;
}

Process::Process(uint32_t id, std::string name, uint32_t instructionCount)
    : id_(id),
      name_(sanitizeOrGenerateName(id, std::move(name))),
      createdAt_(formatTimestamp())
{
    const uint64_t count = (instructionCount == 0) ? 10 : instructionCount;
    instructions_ = generateRandomInstructions(count, count, name_);
}

Process::Process(uint32_t id, std::string name, uint64_t minIns, uint64_t maxIns)
    : id_(id),
      name_(sanitizeOrGenerateName(id, std::move(name))),
      createdAt_(formatTimestamp())
{
    instructions_ = generateRandomInstructions(minIns, maxIns, name_);
}

Process::Process(uint32_t id, std::string name, std::vector<Instruction> instructions)
    : id_(id),
      name_(sanitizeOrGenerateName(id, std::move(name))),
      createdAt_(formatTimestamp()),
      instructions_(std::move(instructions))
{
    if (instructions_.empty())
    {
        instructions_.push_back(Instruction::makePrint());
    }
}

Process::~Process()
{
    if (logFile_.is_open())
        logFile_.close();
}

void Process::markRunning(int core)
{
    core_.store(core);
    state_.store(ProcessState::Running);
    if (startedAt_.empty())
        startedAt_ = formatTimestamp();
}

void Process::enableFileLogging(const std::string& dir)
{
    if (!SchedulerSettings::kEnablePrintFileLogging)
        return;
    logPath_ = dir + "/" + name_ + ".txt";
    fileLoggingEnabled_ = true;
}

// ---------------------------------------------------------------------------
// Memory / Symbol Table (uint16 clamped between [0, 65535])
// ---------------------------------------------------------------------------

void Process::storeVariable(const std::string& var, uint32_t value)
{
    std::lock_guard<std::mutex> lk(memoryMtx_);
    const uint32_t clamped = std::min<uint32_t>(value, 65535U);
    symbolTable_[var] = static_cast<uint16_t>(clamped);
}

uint16_t Process::fetchVariable(const std::string& var)
{
    std::lock_guard<std::mutex> lk(memoryMtx_);
    auto it = symbolTable_.find(var);
    if (it == symbolTable_.end())
    {
        symbolTable_[var] = 0; // Auto-declared with 0 if undeclared
        return 0;
    }
    return it->second;
}

bool Process::hasVariable(const std::string& var) const
{
    std::lock_guard<std::mutex> lk(memoryMtx_);
    return symbolTable_.find(var) != symbolTable_.end();
}

std::unordered_map<std::string, uint16_t> Process::getSymbolTableSnapshot() const
{
    std::lock_guard<std::mutex> lk(memoryMtx_);
    return symbolTable_;
}

uint16_t Process::resolveOperand(const Operand& op)
{
    if (op.isVariable)
        return fetchVariable(op.varName);
    return std::min<uint16_t>(op.literalValue, 65535U);
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

std::string Process::generateLogLine(int core) const
{
    const std::string coreStr = (core >= 0) ? std::to_string(core) : "N/A";
    return name_ + " | (" + formatTimestamp() + ") | Core: " + coreStr + " " +
           std::to_string(executed_.load()) + "/" + std::to_string(totalInstructions());
}

void Process::appendLog(const std::string& logLine)
{
    {
        std::lock_guard<std::mutex> lk(logMtx_);
        logs_.push_back(logLine);
    }

    if (fileLoggingEnabled_)
    {
        if (!logFile_.is_open())
        {
            logFile_.open(logPath_, std::ios::out | std::ios::trunc);
            if (!logFile_.is_open())
            {
                fileLoggingEnabled_ = false;
                return;
            }
            logFile_ << "Process name: " << name_ << "\nLogs:\n\n";
        }
        logFile_ << logLine << "\n";
    }
}

std::vector<std::string> Process::getLogs() const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    return logs_;
}

// ---------------------------------------------------------------------------
// Sleep Management
// ---------------------------------------------------------------------------

void Process::putToSleep(uint8_t ticks)
{
    sleepTicksRemaining_ = ticks;
    if (ticks > 0)
        state_.store(ProcessState::Waiting);
    else
        state_.store(ProcessState::Ready);
}

void Process::tickSleep()
{
    if (state_.load() != ProcessState::Waiting)
        return;

    if (sleepTicksRemaining_ > 0)
    {
        --sleepTicksRemaining_;
        if (sleepTicksRemaining_ == 0)
        {
            state_.store(ProcessState::Ready);
        }
    }
    else
    {
        state_.store(ProcessState::Ready);
    }
}

// ---------------------------------------------------------------------------
// Instruction Pipeline: Fetch, Decode, Execute
// ---------------------------------------------------------------------------

const Instruction& Process::fetchInstruction() const
{
    if (!loopStack_.empty())
    {
        const auto& top = loopStack_.back();
        return top.instructions->at(top.pc);
    }
    const uint32_t idx = executed_.load();
    if (idx < instructions_.size())
        return instructions_[idx];

    static const Instruction kFinishedFallback = Instruction::makePrint();
    return kFinishedFallback;
}

DecodedInstruction Process::decodeInstruction(const Instruction& inst)
{
    DecodedInstruction dec;
    dec.type = inst.type;

    switch (inst.type)
    {
    case InstructionType::Print: {
        std::string text = inst.message;
        if (text.empty() && inst.printVar.empty())
        {
            text = "Hello world from " + name_ + "!";
        }
        else if (!inst.printVar.empty())
        {
            const uint16_t v = fetchVariable(inst.printVar);
            text += std::to_string(v);
        }
        dec.printOutput = std::move(text);
        break;
    }
    case InstructionType::Declare: {
        dec.targetVar = inst.declareVar;
        dec.computedValue = std::min<uint16_t>(inst.declareValue, 65535U);
        break;
    }
    case InstructionType::Add: {
        dec.targetVar = inst.destVar;
        const uint32_t a = resolveOperand(inst.op1);
        const uint32_t b = resolveOperand(inst.op2);
        const uint32_t sum = a + b;
        dec.computedValue = static_cast<uint16_t>(std::min<uint32_t>(sum, 65535U));
        break;
    }
    case InstructionType::Subtract: {
        dec.targetVar = inst.destVar;
        const int32_t a = resolveOperand(inst.op1);
        const int32_t b = resolveOperand(inst.op2);
        const int32_t diff = a - b;
        const int32_t clamped = std::clamp<int32_t>(diff, 0, 65535);
        dec.computedValue = static_cast<uint16_t>(clamped);
        break;
    }
    case InstructionType::Sleep: {
        dec.sleepTicks = inst.sleepTicks;
        break;
    }
    case InstructionType::For: {
        dec.forBody = &inst.nestedInstructions;
        dec.forRepeats = inst.repeats;
        break;
    }
    }
    return dec;
}

void Process::executeInstruction(const DecodedInstruction& decoded, uint32_t core)
{
    switch (decoded.type)
    {
    case InstructionType::Print: {
        const std::string line = "(" + formatTimestamp() + ") Core:" + std::to_string(core) +
                                 " \"" + decoded.printOutput + "\"";
        appendLog(line);
        advanceInstructionPointer();
        break;
    }
    case InstructionType::Declare:
    case InstructionType::Add:
    case InstructionType::Subtract: {
        storeVariable(decoded.targetVar, decoded.computedValue);
        advanceInstructionPointer();
        break;
    }
    case InstructionType::Sleep: {
        putToSleep(decoded.sleepTicks);
        advanceInstructionPointer();
        break;
    }
    case InstructionType::For: {
        if (decoded.forBody && !decoded.forBody->empty() && decoded.forRepeats > 0)
        {
            LoopFrame frame;
            frame.instructions = decoded.forBody;
            frame.totalRepeats = decoded.forRepeats;
            frame.currentRepeat = 0;
            frame.pc = 0;
            loopStack_.push_back(frame);
        }
        else
        {
            advanceInstructionPointer();
        }
        break;
    }
    }
}

void Process::advanceInstructionPointer()
{
    if (!loopStack_.empty())
    {
        auto& top = loopStack_.back();
        ++top.pc;
        if (top.pc >= top.instructions->size())
        {
            ++top.currentRepeat;
            top.pc = 0;
            if (top.currentRepeat >= top.totalRepeats)
            {
                loopStack_.pop_back();
                advanceInstructionPointer();
            }
        }
    }
    else
    {
        executed_.fetch_add(1);
    }
}

void Process::executeNext(uint32_t core, uint64_t delayPerExec)
{
    if (isFinished())
    {
        state_.store(ProcessState::Finished);
        return;
    }

    // Process is sleeping: tick sleep duration
    if (state_.load() == ProcessState::Waiting)
    {
        tickSleep();
        return;
    }

    // Busy-wait delay cycles
    if (delayRemaining_ > 0)
    {
        --delayRemaining_;
        return;
    }

    // Fetch -> Decode -> Execute
    const Instruction& inst = fetchInstruction();
    const DecodedInstruction dec = decodeInstruction(inst);
    executeInstruction(dec, core);

    // Self-transition to FINISHED once all instructions have completed
    if (loopStack_.empty() && executed_.load() >= totalInstructions())
    {
        state_.store(ProcessState::Finished);
        if (logFile_.is_open())
            logFile_.close();
    }
    else if (state_.load() != ProcessState::Waiting)
    {
        delayRemaining_ = delayPerExec;
    }
}

// ---------------------------------------------------------------------------
// Random Instruction Generator
// ---------------------------------------------------------------------------

std::vector<Instruction> Process::generateRandomInstructions(
    uint64_t minIns, uint64_t maxIns, const std::string& procName, uint32_t maxDepth)
{
    static thread_local std::mt19937_64 rng(std::random_device{}());

    const uint64_t low = std::max<uint64_t>(1ULL, minIns);
    const uint64_t high = std::max<uint64_t>(low, maxIns);
    std::uniform_int_distribution<uint64_t> countDist(low, high);
    const uint64_t totalCount = countDist(rng);

    const std::vector<std::string> varPool = {"x", "y", "z", "count"};
    std::uniform_int_distribution<size_t> varDist(0, varPool.size() - 1);
    std::uniform_int_distribution<int> typeDist(0, (maxDepth > 0) ? 5 : 4);
    std::uniform_int_distribution<uint16_t> valDist(1, 100);
    std::uniform_int_distribution<int> boolDist(0, 1);
    std::uniform_int_distribution<int> sleepDist(1, 5);

    std::vector<Instruction> result;
    result.reserve(static_cast<size_t>(totalCount));

    // Ensure initial declaration
    result.push_back(Instruction::makeDeclare(varPool[0], valDist(rng)));

    for (uint64_t i = 1; i < totalCount; ++i)
    {
        const int choice = typeDist(rng);
        switch (choice)
        {
        case 0: { // PRINT
            if (boolDist(rng) == 0)
            {
                result.push_back(Instruction::makePrint("Hello world from " + procName + "!"));
            }
            else
            {
                const std::string& v = varPool[varDist(rng)];
                result.push_back(Instruction::makePrint("Value of " + v + ": ", v));
            }
            break;
        }
        case 1: { // DECLARE
            const std::string& v = varPool[varDist(rng)];
            result.push_back(Instruction::makeDeclare(v, valDist(rng)));
            break;
        }
        case 2: { // ADD
            const std::string& dest = varPool[varDist(rng)];
            Operand op1 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            Operand op2 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            result.push_back(Instruction::makeAdd(dest, op1, op2));
            break;
        }
        case 3: { // SUBTRACT
            const std::string& dest = varPool[varDist(rng)];
            Operand op1 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            Operand op2 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            result.push_back(Instruction::makeSubtract(dest, op1, op2));
            break;
        }
        case 4: { // SLEEP
            result.push_back(Instruction::makeSleep(sleepDist(rng)));
            break;
        }
        case 5: { // FOR (nested up to maxDepth)
            std::vector<Instruction> loopBody;
            loopBody.push_back(Instruction::makeAdd(varPool[0], Operand::fromVariable(varPool[0]), Operand::fromValue(1)));
            loopBody.push_back(Instruction::makePrint("Loop iteration in " + procName));
            result.push_back(Instruction::makeFor(std::move(loopBody), 2));
            break;
        }
        }
    }

    return result;
}