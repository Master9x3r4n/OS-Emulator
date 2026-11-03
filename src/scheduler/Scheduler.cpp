#include <scheduler/Scheduler.hpp>

#include <iostream>
#include <system_error>

Scheduler::Scheduler(const Config& config, std::string logDir,
                     std::chrono::milliseconds tickDuration)
    : policy_(config.scheduler == "rr" ? Policy::RoundRobin : Policy::FCFS),
      numCores_(static_cast<uint32_t>(config.numCpu)),
      quantumCycles_(config.quantumCycles),
      batchProcessFreq_(config.batchProcessFreq),
      delayPerExec_(config.delayPerExec),
      minIns_(config.minIns),
      maxIns_(config.maxIns),
      tickDuration_(tickDuration),
      slots_(numCores_),
      quantumUsed_(numCores_, 0)
{
}

Scheduler::~Scheduler()
{
    stop();
}

bool Scheduler::start()
{
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (running_)
            return true;
        running_ = true;
    }

    try
    {
        workers_.reserve(numCores_);
        for (uint32_t c = 0; c < numCores_; ++c)
            workers_.emplace_back(&Scheduler::workerLoop, this, c);
        schedulerThread_ = std::thread(&Scheduler::schedulerLoop, this);
    }
    catch (const std::system_error& e)
    {
        std::cerr << "Error: failed to start scheduler threads: " << e.what() << "\n";
        stop();
        return false;
    }
    return true;
}

void Scheduler::stop()
{
    {
        std::lock_guard<std::mutex> lk(mtx_);
        running_ = false;
        generating_ = false;
    }
    cvTick_.notify_all();
    cvDone_.notify_all();
    cvSleep_.notify_all();
    joinAll();
}

void Scheduler::joinAll()
{
    if (schedulerThread_.joinable())
        schedulerThread_.join();
    for (auto& w : workers_)
        if (w.joinable())
            w.join();
    workers_.clear();
}


// Generator Controls
bool Scheduler::startGenerator()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (generating_)
    {
        return false;
    }
    generating_ = true;
    return true;
}

bool Scheduler::stopGenerator()
{
    std::lock_guard<std::mutex> lk(mtx_);
    if (!generating_)
    {
        return false;
    }
    generating_ = false;
    return true;
}

bool Scheduler::isGenerating() const
{
    std::lock_guard<std::mutex> lk(mtx_);
    return generating_;
}

// Process Creation & Lookup
std::shared_ptr<Process> Scheduler::createProcess(const std::string& name,
                                                  std::string* error,
                                                  uint32_t instructionCount)
{
    std::lock_guard<std::mutex> lk(mtx_);
    return createProcessLocked(name, error, instructionCount);
}

std::shared_ptr<Process> Scheduler::createProcess(const std::string& name,
                                                  uint32_t instructionCount)
{
    return createProcess(name, nullptr, instructionCount);
}

std::shared_ptr<Process> Scheduler::createProcessLocked(const std::string& name,
                                                        std::string* error,
                                                        uint32_t instructionCount)
{
    std::string finalName = name;
    const uint32_t procId = nextId_;

    if (!finalName.empty())
    {
        if (finalName.length() > Process::kMaxNameLength)
        {
            if (error)
            {
                *error = "Process name '" + finalName + "' exceeds maximum length of " +
                         std::to_string(Process::kMaxNameLength) + " characters.";
            }
            return nullptr;
        }

        if (nameExistsLocked(finalName))
        {
            if (error)
            {
                *error = "Process with name '" + finalName + "' already exists.";
            }
            return nullptr;
        }
    }
    else
    {
        while (true)
        {
            std::string candidate = "process" + std::to_string(procId);
            if (candidate.length() > Process::kMaxNameLength)
            {
                candidate = candidate.substr(0, Process::kMaxNameLength);
            }

            if (!nameExistsLocked(candidate))
            {
                finalName = candidate;
                break;
            }
        }
    }

    ++nextId_;

    std::shared_ptr<Process> p;
    if (instructionCount > 0)
    {
        p = std::make_shared<Process>(procId, finalName, instructionCount);
    }
    else
    {
        p = std::make_shared<Process>(procId, finalName, minIns_, maxIns_);
    }

    ready_.push_back(p);
    return p;
}

bool Scheduler::nameExistsLocked(const std::string& name) const
{
    for (const auto& p : ready_)
        if (p->name() == name) return true;
    for (const auto& p : slots_)
        if (p && p->name() == name) return true;
    for (const auto& p : waiting_)
        if (p && p->name() == name) return true;
    for (const auto& p : finished_)
        if (p->name() == name) return true;
    return false;
}

std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) const
{
    if (name.empty() || name.length() > Process::kMaxNameLength)
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& p : slots_)
        if (p && p->name() == name) return p;
    for (const auto& p : ready_)
        if (p && p->name() == name) return p;
    for (const auto& p : waiting_)
        if (p && p->name() == name) return p;
    for (const auto& p : finished_)
        if (p && p->name() == name) return p;
    return nullptr;
}

// Scheduler Loop & Synchronization
void Scheduler::schedulerLoop()
{
    std::unique_lock<std::mutex> lk(mtx_);
    while (running_)
    {
        // 1. Generate batch dummy processes per tick if enabled
        if (generating_)
        {
            for (uint64_t i = 0; i < batchProcessFreq_; ++i)
            {
                createProcessLocked();
            }
        }

        // 2. Dispatch ready processes to idle cores
        dispatchLocked();

        // 3. Wall-clock tick duration (early wakeup on shutdown)
        cvSleep_.wait_for(lk, tickDuration_, [this] { return !running_; });
        if (!running_)
            break;

        // 4. Release worker threads for 1 tick
        pending_ = numCores_;
        ++generation_;
        cvTick_.notify_all();
        cvDone_.wait(lk, [this] { return pending_ == 0 || !running_; });
        if (!running_)
            break;

        ++cpuCycles_;
        collectLocked();
    }
}

void Scheduler::dispatchLocked()
{
    for (uint32_t c = 0; c < numCores_; ++c)
    {
        if (slots_[c] || ready_.empty())
            continue;

        std::shared_ptr<Process> p = std::move(ready_.front());
        ready_.pop_front();
        p->markRunning(static_cast<int>(c));
        slots_[c] = std::move(p);
        quantumUsed_[c] = 0;
    }
}

void Scheduler::collectLocked()
{
    // 1. Tick sleeping processes; when awake, return to ready queue
    for (auto it = waiting_.begin(); it != waiting_.end(); )
    {
        (*it)->tickSleep();
        if ((*it)->state() == ProcessState::Ready)
        {
            ready_.push_back(std::move(*it));
            it = waiting_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // 2. Collect finished or waiting processes from CPU slots
    for (uint32_t c = 0; c < numCores_; ++c)
    {
        std::shared_ptr<Process>& p = slots_[c];
        if (!p)
            continue;

        if (p->isFinished())
        {
            p->setCore(-1);
            p->setState(ProcessState::Finished);
            finished_.push_back(std::move(p));
            p.reset();
            quantumUsed_[c] = 0;
        }
        else if (p->state() == ProcessState::Waiting)
        {
            p->setCore(-1);
            if (p->sleepTicksRemaining() == 0)
            {
                p->setState(ProcessState::Ready);
                ready_.push_back(std::move(p));
            }
            else
            {
                waiting_.push_back(std::move(p));
            }
            p.reset();
            quantumUsed_[c] = 0;
        }
        else if (policy_ == Policy::RoundRobin && quantumUsed_[c] >= quantumCycles_)
        {
            if (!ready_.empty())
            {
                p->setCore(-1);
                p->setState(ProcessState::Ready);
                ready_.push_back(std::move(p));
                p.reset();
            }
            quantumUsed_[c] = 0;
        }
    }
}

void Scheduler::workerLoop(uint32_t core)
{
    uint64_t seen = 0;
    std::unique_lock<std::mutex> lk(mtx_);
    while (true)
    {
        cvTick_.wait(lk, [&] { return !running_ || generation_ != seen; });
        if (!running_)
            return;
        seen = generation_;

        std::shared_ptr<Process> proc = slots_[core];
        lk.unlock();
        if (proc)
        {
            proc->executeNext(core, delayPerExec_);
            ++quantumUsed_[core];
        }
        lk.lock();

        if (--pending_ == 0)
            cvDone_.notify_one();
    }
}

// Snapshot
Scheduler::ProcessInfo Scheduler::describeLocked(const Process& p) const
{
    ProcessInfo info;
    info.name = p.name();
    info.startedAt = p.startedAt();
    info.core = p.core();
    info.executed = p.executedInstructions();
    info.total = p.totalInstructions();
    return info;
}

Scheduler::Snapshot Scheduler::snapshot() const
{
    std::lock_guard<std::mutex> lk(mtx_);

    Snapshot s;
    s.cpuCycles = cpuCycles_;
    s.totalCores = numCores_;

    for (uint32_t c = 0; c < numCores_; ++c)
    {
        if (!slots_[c])
            continue;
        ProcessInfo info = describeLocked(*slots_[c]);
        info.core = static_cast<int>(c);
        s.running.push_back(std::move(info));
    }

    s.coresUsed = static_cast<uint32_t>(s.running.size());
    s.utilization = numCores_ == 0 ? 0.0 : 100.0 * s.coresUsed / numCores_;

    for (const auto& p : finished_)
        s.finished.push_back(describeLocked(*p));

    return s;
}