#include <process/Process.hpp>
#include <utils/Time.hpp>

Process::Process(uint32_t id, std::string name, uint32_t instructionCount)
    : id_(id),
      name_(std::move(name)),
      createdAt_(formatTimestamp()),
      instructions_(instructionCount)
{
}

Process::Process(uint32_t id, std::string name, std::vector<Instruction> instructions)
    : id_(id),
      name_(std::move(name)),
      createdAt_(formatTimestamp()),
      instructions_(std::move(instructions))
{
}

void Process::markRunning(int core)
{
    core_.store(core);
    state_.store(ProcessState::Running);
    if (startedAt_.empty())
        startedAt_ = formatTimestamp();
}

uint16_t Process::getVariable(const std::string& varName)
{
    std::lock_guard<std::mutex> lk(symbolMtx_);
    auto it = symbolTable_.find(varName);
    if (it == symbolTable_.end())
    {
        // Auto-declare undeclared variable as 0 per AC
        symbolTable_[varName] = 0;
        return 0;
    }
    return it->second;
}

void Process::setVariable(const std::string& varName, uint16_t value)
{
    std::lock_guard<std::mutex> lk(symbolMtx_);
    symbolTable_[varName] = value;
}

bool Process::hasVariable(const std::string& varName) const
{
    std::lock_guard<std::mutex> lk(symbolMtx_);
    return symbolTable_.find(varName) != symbolTable_.end();
}

std::unordered_map<std::string, uint16_t> Process::getSymbolTableSnapshot() const
{
    std::lock_guard<std::mutex> lk(symbolMtx_);
    return symbolTable_;
}

std::vector<ProcessLog> Process::getLogs() const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    return logs_;
}

std::vector<std::string> Process::getFormattedLogs() const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    std::vector<std::string> out;
    out.reserve(logs_.size());
    for (const auto& log : logs_)
        out.push_back(log.format());
    return out;
}

std::vector<std::string> Process::getFormattedLogsSince(size_t index) const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    std::vector<std::string> out;
    if (index >= logs_.size())
        return out;

    out.reserve(logs_.size() - index);
    for (size_t i = index; i < logs_.size(); ++i)
        out.push_back(logs_[i].format());
    return out;
}

size_t Process::getLogCount() const
{
    std::lock_guard<std::mutex> lk(logMtx_);
    return logs_.size();
}

void Process::executeNext(uint32_t core, uint64_t delayPerExec)
{
    if (isFinished())
        return;

    // Busy-wait: the process stays on the core and burns a tick.
    if (delayRemaining_ > 0)
    {
        --delayRemaining_;
        return;
    }

    const uint32_t currentIdx = executed_.load();
    if (currentIdx < instructions_.size())
    {
        switch (instructions_[currentIdx].type)
        {
        case Instruction::Type::Print:
            executePrint(core);
            break;
        }
    }

    const uint32_t done = executed_.fetch_add(1) + 1;
    if (done < totalInstructions())
    {
        delayRemaining_ = delayPerExec;
    }
}

void Process::executePrint(uint32_t core)
{
    const uint32_t currentIdx = executed_.load();
    const Instruction& ins = instructions_[currentIdx];

    std::string resolvedMsg;

    if (!ins.varName.empty())
    {
        // Query symbol table; returns 0 and auto-declares if undeclared
        const uint16_t val = getVariable(ins.varName);
        if (ins.hasCustomMessage)
            resolvedMsg = ins.message + std::to_string(val);
        else
            resolvedMsg = std::to_string(val);
    }
    else if (ins.hasCustomMessage)
    {
        resolvedMsg = ins.message;
    }
    else
    {
        resolvedMsg = "Hello world from " + name_ + "!";
    }

    ProcessLog entry{formatTimestamp(), core, std::move(resolvedMsg)};

    {
        std::lock_guard<std::mutex> lk(logMtx_);
        logs_.push_back(std::move(entry));
    }
    // Worker threads NEVER call std::cout directly.
    // Console output occurs only through the screen interface when attached.
}