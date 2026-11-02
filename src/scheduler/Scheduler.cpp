#include <scheduler/Scheduler.hpp>

#include <filesystem>
#include <iostream>
#include <system_error>

Scheduler::Scheduler(const Config& config, std::string logDir,
                     std::chrono::milliseconds tickDuration)
    : policy_(config.scheduler == "rr" ? Policy::RoundRobin : Policy::FCFS),
      numCores_(static_cast<uint32_t>(config.numCpu)),
      quantumCycles_(config.quantumCycles),
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

std::shared_ptr<Process> Scheduler::createProcess(const std::string& name,
                                                  uint32_t instructionCount)
{
    std::lock_guard<std::mutex> lk(mtx_);

    std::string finalName = name;

    if (!finalName.empty())
    {
        // 1. Truncate user name to 14 chars max BEFORE checking uniqueness
        if (finalName.length() > Process::kMaxNameLength)
        {
            finalName = finalName.substr(0, Process::kMaxNameLength);
        }

        // Check if the 14-char name already exists
        if (nameExistsLocked(finalName))
        {
            return nullptr;
        }
    }
    else
    {
        // 2. Generate a name that is guaranteed NOT to collide with any user name
        while (true)
        {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "p%02u", nextAutoNameId_++);
            std::string candidate(buf);
            if (!nameExistsLocked(candidate))
            {
                finalName = candidate;
                break;
            }
        }
    }

    std::shared_ptr<Process> p;
    if (instructionCount > 0)
    {
        p = std::make_shared<Process>(nextId_++, finalName, instructionCount);
    }
    else
    {
        p = std::make_shared<Process>(nextId_++, finalName, minIns_, maxIns_);
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

// ---------------------------------------------------------------------------
// Scheduler thread (tick clock)
// ---------------------------------------------------------------------------

void Scheduler::schedulerLoop()
{
    std::unique_lock<std::mutex> lk(mtx_);
    while (running_)
    {
        dispatchLocked();

        // One tick of wall-clock time; wakes early on shutdown.
        cvSleep_.wait_for(lk, tickDuration_, [this] { return !running_; });
        if (!running_)
            break;

        // Release every worker for exactly one tick and wait for all of them.
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

// Fill idle cores from the front of the shared FIFO ready queue.
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

// After a tick: retire finished processes and, for round robin, rotate out
// processes whose quantum expired. FCFS never preempts.
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
            p->setState(ProcessState::Finished);
            finished_.push_back(std::move(p));
            p.reset();
            quantumUsed_[c] = 0;
        }
        else if (p->state() == ProcessState::Waiting)
        {
            // Process relinquished CPU due to SLEEP(X)
            waiting_.push_back(std::move(p));
            p.reset();
            quantumUsed_[c] = 0;
        }
        else if (policy_ == Policy::RoundRobin && quantumUsed_[c] >= quantumCycles_)
        {
            if (!ready_.empty())
            {
                p->setState(ProcessState::Ready);
                ready_.push_back(std::move(p));
                p.reset();
            }
            quantumUsed_[c] = 0;
        }
    }
}

// ---------------------------------------------------------------------------
// Worker threads (one per core)
// ---------------------------------------------------------------------------

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
            // This worker exclusively owns `proc` for the duration of the tick.
            proc->executeNext(core, delayPerExec_);
            ++quantumUsed_[core];
        }
        lk.lock();

        if (--pending_ == 0)
            cvDone_.notify_one();
    }
}

// ---------------------------------------------------------------------------
// Snapshot
// ---------------------------------------------------------------------------

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


// Look up any process
std::shared_ptr<Process> Scheduler::findProcess(const std::string& name) const
{
    std::string searchName = name;
    if (searchName.length() > Process::kMaxNameLength)
    {
        searchName = searchName.substr(0, Process::kMaxNameLength);
    }

    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& p : slots_)
        if (p && p->name() == searchName) return p;
    for (const auto& p : ready_)
        if (p && p->name() == searchName) return p;
    for (const auto& p : waiting_)
        if (p && p->name() == searchName) return p;
    for (const auto& p : finished_)
        if (p && p->name() == searchName) return p;
    return nullptr;
}