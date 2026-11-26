#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <cstdio>

#include <utils/Input.hpp>
#include <utils/Display.hpp>
#include <utils/Reports.hpp>
#include <config/Config.hpp>
#include <memory/BackingStore.hpp>
#include <memory/MemoryManager.hpp>
#include <scheduler/Scheduler.hpp>

Config config;
std::shared_ptr<BackingStore> backingStore;
std::shared_ptr<MemoryManager> memoryManager;
std::unique_ptr<Scheduler> scheduler;

bool initializeEmulator(const std::string& configPath) {
    if (scheduler) {
        std::cout << "Program is already initialized.\n";
        return true;
    }

    if (!config.initialize(configPath))
        return false;

    backingStore = std::make_shared<BackingStore>(std::string(PROJECT_DIR) + "/logs/csopesy-backing-store.txt");
    memoryManager = std::make_shared<MemoryManager>(config, backingStore);

    scheduler = std::make_unique<Scheduler>(config, std::string(PROJECT_DIR) + "/logs", memoryManager);
    if (!scheduler->start()) {
        scheduler.reset();
        memoryManager.reset();
        backingStore.reset();
        return false;
    }

    if (SchedulerSettings::kSeedTestProcesses) {
        for (uint32_t i = 1; i <= SchedulerSettings::kTestProcessCount; ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "process%02u", i);
            scheduler->createProcess(name, SchedulerSettings::kTestProcessInstructions);
        }
    }
    return true;
}

struct Command {
    bool requiresConfig;
    std::function<void(const std::vector<std::string>&)> handler;
};

void renderProcessScreen(const std::shared_ptr<Process>& proc)
{
    std::cout << "Process: " << proc->name() << "\n"
              << "ID: " << proc->id() << "\n";
    std::cout << "Logs:\n";

    const auto logs = proc->getLogs();
    for (const auto& line : logs) {
        std::cout << line << "\n";
    }

    std::cout << "\n";

    if (!proc->isFinished()) {
        std::cout << "Current instruction: " << proc->executedInstructions() << "\n"
                  << "Lines of code: " << proc->totalInstructions() << "\n\n";
    }
    else {
        std::cout << "Finished!\n\n";
    }
}

void attachProcessScreen(const std::shared_ptr<Process>& proc)
{
    proc->setAttached(true);
    clearScreen();
    renderProcessScreen(proc);

    while (true) {
        std::vector<std::string> input = getInput();

        if (std::cin.eof()) {
            break;
        }

        if (input.empty() || input[0].empty()) {
            continue;
        }

        const std::string& cmd = input[0];
        if (cmd == "help") {
            displayProcessHelp();
        }
        else if (cmd == "exit") {
            break;
        }
        else if (cmd == "process-smi") {
            renderProcessScreen(proc);
        }
        else if (cmd == "clear") {
            clearScreen();
        }
        else {
            std::cout << "Unknown command. Type 'help' for list of valid commands.\n";
        }
    }

    proc->setAttached(false);
    
    if (!std::cin.eof()) {
        clearScreen();
        displayConsole();
    }
}

void handleScreenCommand(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayScreenHelp();
        return;
    }

    const std::string& flag = args[1];
    if (flag == "-ls") {
        GenerateReport(scheduler->snapshot());
    }
    else if (flag == "-s") {
        if (args.size() < 3) {
            displayScreenHelp();
            return;
        }

        std::string procName = args[2];
        std::string error;
        auto p = scheduler->createProcess(procName, &error);
        if (p) {
            attachProcessScreen(p);
        } else {
            std::cout << "Error: " << error << "\n";
        }
    }
    else if (flag == "-r") {
        if (args.size() < 3) {
            displayScreenHelp();
            return;
        }
        const std::string& procName = args[2];
        auto proc = scheduler->findProcess(procName);
        if (!proc || proc->isFinished()) {
            std::cout << "Process " << procName << " not found.\n";
            return;
        }
        attachProcessScreen(proc);
    }
    else {
        displayScreenHelp();
    }
}

int main() {
    bool isProgramRunning = true;
    bool isProgramInitialized = false;
    std::vector<std::string> commandInput;

    const std::string configPath = std::string(PROJECT_DIR) + "/config/config.txt";

    std::unordered_map<std::string, Command> commandRegistry = {
        {"initialize",      {false, [&](const auto&) { isProgramInitialized = initializeEmulator(configPath); }}},
        {"help",            {false, [&](const auto&) { displayHelp(isProgramInitialized); }}},
        {"clear",           {false, [](const auto&)  { clearScreen(); displayConsole(); }}},
        {"exit",            {false, [&](const auto&) { isProgramRunning = false; }}},

        {"report-util",     {true,  [](const auto&)  {
            GenerateReport(scheduler->snapshot(), true);
        }}},
        {"scheduler-start", {true, [](const auto&) {
            if (scheduler->startGenerator()) {
                std::cout << "Scheduler generation started.\n";
            } else {
                std::cout << "Scheduler generation is already running.\n";
            }
        }}},
        {"scheduler-stop", {true, [](const auto&) {
            if (scheduler->stopGenerator()) {
                std::cout << "Scheduler generation stopped.\n";
            } else {
                std::cout << "Scheduler generation is not running.\n";
            }
        }}},
        {"screen",          {true,  handleScreenCommand}}
    };

    clearScreen();
    displayConsole();
    std::cout << "Type 'help' for a list of commands\n";

    while (isProgramRunning) {
        if (std::cin.eof()) {
            isProgramRunning = false;
            break;
        }
        
        commandInput = getInput();

        if (std::cin.eof()) {
            isProgramRunning = false;
            break;
        }

        if (commandInput.empty() || commandInput[0].empty() ) {
            continue;
        }

        const std::string& rootCommand = commandInput[0];
        auto it = commandRegistry.find(rootCommand);

        if (it == commandRegistry.end()) {
            std::cout << "Unknown command. Type 'help' for list of valid commands.\n";
        }
        else {
            const Command& cmd = it->second;
            if (cmd.requiresConfig && !isProgramInitialized) {
                std::cout << "Error occurred. Initialize program first. Type 'help' for list of valid commands.\n";
            } else {
                cmd.handler(commandInput);
            }
        }
    }

    if (scheduler)
        scheduler->stop();

    clearScreen();
    std::cout << "Terminating program...";
    return 0;
}