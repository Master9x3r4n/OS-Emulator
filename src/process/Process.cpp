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
    totalInstructions_ = countTotalInstructions(instructions_);
}

Process::Process(uint32_t id, std::string name, uint64_t minIns, uint64_t maxIns)
    : id_(id),
      name_(sanitizeOrGenerateName(id, std::move(name))),
      createdAt_(formatTimestamp())
{
    instructions_ = generateRandomInstructions(minIns, maxIns, name_);
    totalInstructions_ = countTotalInstructions(instructions_);
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
    totalInstructions_ = countTotalInstructions(instructions_);
}

Process::~Process() = default;

void Process::markRunning(int core)
{
    core_.store(core);
    state_.store(ProcessState::Running);
    if (startedAt_.empty())
        startedAt_ = formatTimestamp();
}

// Memory / Symbol Table (uint16 clamped between [0, 65535])
void Process::declareVariable(const std::string& var, int64_t value)
{
    symbolTable_.declare(var, value);
}

uint16_t Process::readVariable(const std::string& var)
{
    return symbolTable_.read(var);
}

void Process::writeVariable(const std::string& var, int64_t value)
{
    symbolTable_.write(var, value);
}

void Process::storeVariable(const std::string& var, int64_t value)
{
    symbolTable_.write(var, value);
}

uint16_t Process::fetchVariable(const std::string& var)
{
    return symbolTable_.read(var);
}

bool Process::hasVariable(const std::string& var) const
{
    return symbolTable_.has(var);
}

std::unordered_map<std::string, uint16_t> Process::getSymbolTableSnapshot() const
{
    return symbolTable_.snapshot();
}

uint16_t Process::resolveOperand(const Operand& op)
{
    if (op.isVariable)
        return fetchVariable(op.varName);
    return SymbolTable::clamp(op.literalValue);
}

// In-Memory Log Buffer & Formatting

std::string Process::generateLogLine(int core, const std::string& message, std::chrono::system_clock::time_point tp) const
{
    const std::string coreStr = (core >= 0) ? std::to_string(core) : "N/A";
    std::string text = message.empty() ? ("Hello world from " + name_ + "!") : message;
    if (text.empty() || text.front() != '"') {
        text = "\"" + text + "\"";
    }
    return name_ + " " + "(" + formatTimestamp(tp) + ") Core: " + coreStr + " " + text ;
}

void Process::appendLog(const std::string& logLine)
{
    std::lock_guard<std::mutex> lk(logMtx_);
    if (maxLogs_ > 0)
    {
        while (logs_.size() >= maxLogs_)
        {
            logs_.pop_front();
        }
        logs_.push_back(logLine);
    }
}

std::vector<std::string> Process::getLogs() const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    return std::vector<std::string>(logs_.begin(), logs_.end());
}

size_t Process::logCount() const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    return logs_.size();
}

void Process::clearLogs()
{
    std::lock_guard<std::mutex> lk(logMtx_);
    logs_.clear();
}

size_t Process::maxLogCapacity() const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    return maxLogs_;
}

void Process::setMaxLogCapacity(size_t cap)
{
    std::lock_guard<std::mutex> lk(logMtx_);
    maxLogs_ = cap;
    while (logs_.size() > maxLogs_)
    {
        logs_.pop_front();
    }
}

// Sleep Management

void Process::putToSleep(uint8_t ticks)
{
    sleepTicksRemaining_.store(ticks);
    state_.store(ProcessState::Waiting);
}

void Process::tickSleep()
{
    if (state_.load() != ProcessState::Waiting)
        return;

    uint32_t current = sleepTicksRemaining_.load();
    if (current > 0)
    {
        if (sleepTicksRemaining_.fetch_sub(1) == 1)
        {
            state_.store(ProcessState::Ready);
        }
    }
    else
    {
        state_.store(ProcessState::Ready);
    }
}

// Fetch, Decode, Execute pipeline

const Instruction* Process::fetch(uint32_t line) const
{
    if (line < instructions_.size())
        return &instructions_[line];
    return nullptr;
}

const Instruction* Process::fetchInstruction() const
{
    if (!loopStack_.empty())
    {
        const auto& top = loopStack_.back();
        if (top.instructions && top.pc < top.instructions->size())
            return &top.instructions->at(top.pc);
        return nullptr;
    }
    return fetch(static_cast<uint32_t>(pc_));
}

DecodedInstruction Process::decodeInstruction(const Instruction& inst)
{
    DecodedInstruction dec;
    dec.type = inst.type;
    dec.isValid = true;

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
        dec.computedValue = SymbolTable::clamp(inst.declareValue);
        break;
    }
    case InstructionType::Add: {
        dec.targetVar = inst.destVar;
        const int64_t a = resolveOperand(inst.op1);
        const int64_t b = resolveOperand(inst.op2);
        dec.computedValue = SymbolTable::clamp(a + b);
        break;
    }
    case InstructionType::Subtract: {
        dec.targetVar = inst.destVar;
        const int64_t a = resolveOperand(inst.op1);
        const int64_t b = resolveOperand(inst.op2);
        dec.computedValue = SymbolTable::clamp(a - b);
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
    default: {
        dec.type = InstructionType::Invalid;
        dec.isValid = false;
        break;
    }
    }
    return dec;
}

void Process::executeInstruction(const DecodedInstruction& decoded, const ExecutionContext& ctx)
{
    switch (decoded.type)
    {
    case InstructionType::Print:    handlePrint(decoded, ctx);    break;
    case InstructionType::Declare:  handleDeclare(decoded, ctx);  break;
    case InstructionType::Add:      handleAdd(decoded, ctx);      break;
    case InstructionType::Subtract: handleSubtract(decoded, ctx); break;
    case InstructionType::Sleep:    handleSleep(decoded, ctx);    break;
    case InstructionType::For:      handleFor(decoded, ctx);      break;
    default:                        handleInvalid(decoded, ctx);  break;
    }
}

// Handlers

void Process::handlePrint(const DecodedInstruction& decoded, const ExecutionContext& ctx)
{
    // Appends the required format: <process_name> (<date-time>) Core: <core> <msg>
    const std::string line = generateLogLine(ctx.core, decoded.printOutput, ctx.timestamp);
    appendLog(line);
}

void Process::handleDeclare(const DecodedInstruction& decoded, const ExecutionContext& /*ctx*/)
{
    declareVariable(decoded.targetVar, decoded.computedValue);
}

void Process::handleAdd(const DecodedInstruction& decoded, const ExecutionContext& /*ctx*/)
{
    writeVariable(decoded.targetVar, decoded.computedValue);
}

void Process::handleSubtract(const DecodedInstruction& decoded, const ExecutionContext& /*ctx*/)
{
    writeVariable(decoded.targetVar, decoded.computedValue);
}

void Process::handleSleep(const DecodedInstruction& decoded, const ExecutionContext& /*ctx*/)
{
    putToSleep(decoded.sleepTicks);
}

void Process::handleFor(const DecodedInstruction& decoded, const ExecutionContext& /*ctx*/)
{
    const bool canEnter = loopStack_.size() < kMaxLoopDepth   // reject 4th nesting level
                          && decoded.forRepeats > 0           // repeats == 0 skips the body
                          && decoded.forBody
                          && !decoded.forBody->empty();       // nothing to repeat

    if (!canEnter)
    {
        advanceInstructionPointer();
        return;
    }

    LoopFrame frame;
    frame.instructions = decoded.forBody;
    frame.totalRepeats = decoded.forRepeats;
    frame.currentRepeat = 0;
    frame.pc = 0;
    loopStack_.push_back(frame);
}

void Process::handleInvalid(const DecodedInstruction& /*decoded*/, const ExecutionContext& /*ctx*/)
{
    // Gracefully ignore invalid instructions
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
        ++pc_;
    }
}

bool Process::step(uint32_t core)
{
    if (isFinished())
        return false;

    // Drain empty / completed loops or handle FOR headers without consuming worker ticks
    while (true)
    {
        if (loopStack_.empty() && pc_ >= instructions_.size())
        {
            state_.store(ProcessState::Finished);
            return false;
        }

        const Instruction* inst = fetchInstruction();
        if (!inst)
        {
            state_.store(ProcessState::Finished);
            return false;
        }

        // Standard instruction execution (PRINT, DECLARE, ADD, SUBTRACT, SLEEP)
        ExecutionContext ctx{*this, core, std::chrono::system_clock::now()};
        const DecodedInstruction dec = decodeInstruction(*inst);
        executeInstruction(dec, ctx);

        // FOR only enters or skips a loop
        if (dec.type == InstructionType::For)
            continue;

        executed_.fetch_add(1);
        advanceInstructionPointer();

        if (loopStack_.empty() && pc_ >= instructions_.size())
        {
            state_.store(ProcessState::Finished);
        }

        return true;
    }
}

void Process::executeNext(uint32_t core, uint64_t delayPerExec)
{
    if (isFinished())
    {
        state_.store(ProcessState::Finished);
        return;
    }

    // Process is waiting (should not execute on CPU)
    if (state_.load() == ProcessState::Waiting)
    {
        return;
    }

    if (delayRemaining_ > 0)
    {
        --delayRemaining_;
        return;
    }

    const bool executed = step(core);

    if (executed && !isFinished() && state_.load() != ProcessState::Waiting)
    {
        delayRemaining_ = delayPerExec;
    }
}

// Random Instruction Generator

std::vector<Instruction> Process::generateRandomInstructions(
    uint64_t minIns, uint64_t maxIns, const std::string& procName, uint32_t currentDepth)
{
    static thread_local std::mt19937_64 rng(std::random_device{}());

    const uint64_t low = std::max<uint64_t>(1ULL, minIns);
    const uint64_t high = std::max<uint64_t>(low, maxIns);
    std::uniform_int_distribution<unsigned long long> countDist(low, high);
    const uint64_t targetCount = countDist(rng);

    const std::vector<std::string> varPool = {"x", "y", "z", "count"};
    std::uniform_int_distribution<int> varDist(0, static_cast<int>(varPool.size() - 1));
    std::uniform_int_distribution<uint16_t> valDist(1, 100);
    std::uniform_int_distribution<int> boolDist(0, 1);
    std::uniform_int_distribution<int> sleepDist(1, 5);

    std::vector<Instruction> result;
    uint64_t remaining = targetCount;

    // At top level (depth 0), seed with an initial declaration so variables always exist
    if (currentDepth == 0 && remaining > 0)
    {
        result.push_back(Instruction::makeDeclare(varPool[0], valDist(rng)));
        --remaining;
    }

    while (remaining > 0)
    {
        // A FOR loop requires at least 2 remaining instruction executions
        // and nesting depth must be strictly below kMaxLoopDepth (3)
        const bool canMakeFor = (currentDepth < kMaxLoopDepth) && (remaining >= 2);
        std::uniform_int_distribution<int> typeDist(0, canMakeFor ? 5 : 4);
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
            --remaining;
            break;
        }
        case 1: { // DECLARE
            const std::string& v = varPool[varDist(rng)];
            result.push_back(Instruction::makeDeclare(v, valDist(rng)));
            --remaining;
            break;
        }
        case 2: { // ADD
            const std::string& dest = varPool[varDist(rng)];
            Operand op1 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            Operand op2 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            result.push_back(Instruction::makeAdd(dest, op1, op2));
            --remaining;
            break;
        }
        case 3: { // SUBTRACT
            const std::string& dest = varPool[varDist(rng)];
            Operand op1 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            Operand op2 = (boolDist(rng) == 0) ? Operand::fromVariable(varPool[varDist(rng)])
                                               : Operand::fromValue(valDist(rng));
            result.push_back(Instruction::makeSubtract(dest, op1, op2));
            --remaining;
            break;
        }
        case 4: { // SLEEP
            result.push_back(Instruction::makeSleep(static_cast<uint8_t>(sleepDist(rng))));
            --remaining;
            break;
        }
        case 5: { // FOR
            // Repeats in [2, min(3, remaining)]
            const uint64_t maxRepeats = std::min<uint64_t>(3ULL, remaining);
            std::uniform_int_distribution<uint64_t> repeatDist(2ULL, maxRepeats);
            const uint64_t repeats = repeatDist(rng);

            // Maximum instructions in body such that (repeats * bodyBudget <= remaining)
            const uint64_t maxAllowedBudget = remaining / repeats;
            const uint64_t maxBodyBudget = std::min<uint64_t>(8ULL, maxAllowedBudget);
            const uint64_t minBodyBudget = (maxBodyBudget >= 2) ? 2ULL : 1ULL;

            std::uniform_int_distribution<uint64_t> bodyBudgetDist(minBodyBudget, maxBodyBudget);
            const uint64_t bodyBudget = bodyBudgetDist(rng);

            std::vector<Instruction> loopBody = generateRandomInstructions(bodyBudget, bodyBudget, procName, currentDepth + 1);
            result.push_back(Instruction::makeFor(std::move(loopBody), static_cast<uint32_t>(repeats)));

            remaining -= (repeats * bodyBudget);
            break;
        }
        }
    }

    return result;
}

// Count Total Instructions

uint32_t Process::countTotalInstructions(const std::vector<Instruction>& insts, size_t currentDepth)
{
    uint32_t total = 0;
    for (const auto& inst : insts)
    {
        if (inst.type == InstructionType::For)
        {
            if (currentDepth < kMaxLoopDepth && inst.repeats > 0)
            {
                uint32_t bodyCount = countTotalInstructions(inst.nestedInstructions, currentDepth + 1);
                total += bodyCount * inst.repeats;
            }
            // 4th level or repeats == 0 contributes 0 executions
        }
        else
        {
            total += 1;
        }
    }
    return total;
}