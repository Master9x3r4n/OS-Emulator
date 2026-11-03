#include <process/Process.hpp>

#include <cstdio>
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
    instructions_ = InstructionGenerator::generateRandomInstructions(count, count, name_);
    totalInstructions_ = InstructionGenerator::countTotalInstructions(instructions_);
}

Process::Process(uint32_t id, std::string name, uint64_t minIns, uint64_t maxIns)
    : id_(id),
      name_(sanitizeOrGenerateName(id, std::move(name))),
      createdAt_(formatTimestamp())
{
    instructions_ = InstructionGenerator::generateRandomInstructions(minIns, maxIns, name_);
    totalInstructions_ = InstructionGenerator::countTotalInstructions(instructions_);
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
    totalInstructions_ = InstructionGenerator::countTotalInstructions(instructions_);
}

Process::~Process() = default;

void Process::markRunning(int core)
{
    core_.store(core);
    state_.store(ProcessState::Running);
    if (startedAt_.empty())
        startedAt_ = formatTimestamp();
}

// ---------------------------------------------------------------------------
// Symbol Table Delegation
// ---------------------------------------------------------------------------

void Process::declareVariable(const std::string& var, int64_t value) { symbolTable_.declare(var, value); }
uint16_t Process::readVariable(const std::string& var) { return symbolTable_.read(var); }
void Process::writeVariable(const std::string& var, int64_t value) { symbolTable_.write(var, value); }
void Process::storeVariable(const std::string& var, int64_t value) { symbolTable_.write(var, value); }
uint16_t Process::fetchVariable(const std::string& var) { return symbolTable_.read(var); }
bool Process::hasVariable(const std::string& var) const { return symbolTable_.has(var); }
std::unordered_map<std::string, uint16_t> Process::getSymbolTableSnapshot() const { return symbolTable_.snapshot(); }

uint16_t Process::resolveOperand(const Operand& op)
{
    if (op.isVariable)
        return fetchVariable(op.varName);
    return SymbolTable::clamp(op.literalValue);
}

// ---------------------------------------------------------------------------
// Logging Delegation
// ---------------------------------------------------------------------------

std::string Process::generateLogLine(int core, const std::string& message, std::chrono::system_clock::time_point tp) const
{
    return ProcessLogBuffer::formatLogLine(name_, core, message, tp);
}

void Process::appendLog(const std::string& logLine) { logBuffer_.append(logLine); }
std::vector<std::string> Process::getLogs() const { return logBuffer_.getLogs(); }
size_t Process::logCount() const { return logBuffer_.size(); }
void Process::clearLogs() { logBuffer_.clear(); }
size_t Process::maxLogCapacity() const { return logBuffer_.capacity(); }
void Process::setMaxLogCapacity(size_t cap) { logBuffer_.setCapacity(cap); }

// ---------------------------------------------------------------------------
// Sleep Management
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Pipeline: Fetch, Decode, Execute
// ---------------------------------------------------------------------------

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

void Process::handlePrint(const DecodedInstruction& decoded, const ExecutionContext& ctx)
{
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
    const bool canEnter = loopStack_.size() < InstructionGenerator::kMaxLoopDepth
                          && decoded.forRepeats > 0
                          && decoded.forBody
                          && !decoded.forBody->empty();

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

        ExecutionContext ctx{*this, core, std::chrono::system_clock::now()};
        const DecodedInstruction dec = decodeInstruction(*inst);
        executeInstruction(dec, ctx);

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