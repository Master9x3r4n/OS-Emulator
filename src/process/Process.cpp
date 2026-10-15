#include <process/Process.hpp>

#include <scheduler/SchedulerSettings.hpp>
#include <utils/Time.hpp>

Process::Process(uint32_t id, std::string name, uint32_t instructionCount)
    : id_(id),
      name_(std::move(name)),
      createdAt_(formatTimestamp()),
      instructions_(instructionCount)
{
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

    switch (instructions_[executed_.load()].type)
    {
    case Instruction::Type::Print:
        executePrint(core);
        break;
    }

    const uint32_t done = executed_.fetch_add(1) + 1;
    if (done >= totalInstructions())
    {
        if (logFile_.is_open())
            logFile_.close();
    }
    else
    {
        delayRemaining_ = delayPerExec;
    }
}

void Process::executePrint(uint32_t core)
{
    if (!fileLoggingEnabled_)
        return;

    if (!logFile_.is_open())
    {
        logFile_.open(logPath_, std::ios::out | std::ios::trunc);
        if (!logFile_.is_open())
        {
            fileLoggingEnabled_ = false; // workers never write to the console
            return;
        }
        logFile_ << "Process name: " << name_ << "\n"
                 << "Logs:\n\n";
    }

    logFile_ << "(" << formatTimestamp() << ") Core:" << core
             << " \"Hello world from " << name_ << "!\"\n";
}
