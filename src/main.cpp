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
#include <scheduler/Scheduler.hpp>


Config config;
std::unique_ptr<Scheduler> scheduler;

// Loads config.txt, starts the scheduler (scheduler thread + one worker per
// core) and, for the homework test case, seeds the test processes.
// Returns true if the emulator is now initialized.
bool initializeEmulator(const std::string& configPath) {
    if (scheduler) {
        std::cout << "Program is already initialized.\n";
        return true;
    }

    if (!config.initialize(configPath))
        return false;

    scheduler = std::make_unique<Scheduler>(config);
    if (!scheduler->start()) {
        scheduler.reset();
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

void handleScreenCommand(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        displayScreenHelp();
        return;
    }

    const std::string& flag = args[1];
    if (flag == "-ls") {
        GenerateReport(scheduler->snapshot());
    }
    else if (flag == "-s" && args.size() >= 3) {
        // TODO 3: call module to create new process then move to process
        std::cout << "X command recognized. Doing something." << args[2] << "\n";
    }
    else if (flag == "-r" && args.size() >= 3) {
        // TODO 3: call module to move to process
        std::cout << "X command recognized. Doing something." << args[2] << "\n";
    }
    else {
        displayScreenHelp();
    }
}

int main() {
    bool isProgramRunning = true;
    bool isProgramInitialized = false;
    std::vector<std::string> commandInput;

    // Build an absolute path to config.txt from the CMake-provided PROJECT_DIR.
    const std::string configPath = std::string(PROJECT_DIR) + "/config/config.txt";

    // Command Dispatch Registry using Unordered Map
    std::unordered_map<std::string, Command> commandRegistry = {
        // Core Commands
        {"initialize",      {false, [&](const auto&) { isProgramInitialized = initializeEmulator(configPath); }}},
        {"help",            {false, [&](const auto&) { displayHelp(isProgramInitialized); }}},
        {"clear",           {false, [](const auto&)  { }}}, /* Redraw handled by main loop */
        {"exit",            {false, [&](const auto&) { isProgramRunning = false; }}},

        // Process Management Commands
        {"report-util",     {true,  [](const auto&)  {
            GenerateReport(scheduler->snapshot(), true);
        }}},
        {"scheduler-start", {true,  [](const auto&)  { 
            /* TODO 2: call coresponding thread module to generate dummy processes */  
            std::cout << "scheduler-start command recognized. Doing something.\n";
        }}},
        {"scheduler-stop",  {true,  [](const auto&)  { 
            /* TODO 2: call coresponding thread module to stop generating dummy processes */ 
            std::cout << "scheduler-stop command recognized. Doing something.\n"; 
        }}},
        {"screen",          {true,  handleScreenCommand}}
    };

    clearScreen();
    displayConsole();
    std::cout << "Type 'help' for a list of commands\n";

    while (isProgramRunning) {
        commandInput = getInput();
        clearScreen();
        displayConsole();

        // Safety guard against empty inputs
        if (commandInput.empty()) {
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

    // Signal shutdown and join the scheduler + worker threads.
    if (scheduler)
        scheduler->stop();

    clearScreen();
    std::cout << "Terminating program...";
    return 0;
}
