#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include "utils/Input.hpp"
#include "utils/Display.hpp"
#include "utils/Reports.hpp"
#include "config/Config.hpp"

Config config;

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
        // TODO 1: Pass data on generated util report
        GenerateReport();
    } 
    else if (flag == "-s" && args.size() >= 3) {
        // TODO 3: call module to create new process then move to process
        std::cout << "wow you generate process: " << args[2] << "\n";
    } 
    else if (flag == "-r" && args.size() >= 3) {
        // TODO 3: call module to move to process
        std::cout << "wow you access process: " << args[2] << "\n";
    } 
    else {
        displayScreenHelp();
    }
}

int main() {
    bool isProgramRunning = true;
    bool isProgramInitialized = false;
    std::vector<std::string> commandInput;

    // Command Dispatch Registry using Unordered Map (tldr: this is where commands input -> function)
    std::unordered_map<std::string, Command> commandRegistry = {
        // Core Commands
        {"initialize",      {false, [&](const auto&) { isProgramInitialized = config.initialize("configs/config.txt"); }}},
        {"help",            {false, [&](const auto&) { displayHelp(isProgramInitialized); }}},
        {"clear",           {false, [](const auto&)  { }}}, /* Redraw handled by main loop */
        {"exit",            {false, [&](const auto&) { isProgramRunning = false; }}},

        // Process Management Commands
        {"report-util",     {true,  [](const auto&)  { GenerateReport(true /* TODO 1: Pass data on generated util report */); }}},
        {"scheduler-start", {true,  [](const auto&)  { /* TODO 2: call coresponding thread module to generate dummy processes */ }}},
        {"scheduler-stop",  {true,  [](const auto&)  { /* TODO 2: call coresponding thread module to stop generating dummy processes */ }}},
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
            const Command& cmd = it->second; // get handler portion of command iterator
            if (cmd.requiresConfig && !isProgramInitialized) {
                std::cout << "Error occurred. Initialize program first. Type 'help' for list of valid commands.\n";
            } else {
                cmd.handler(commandInput); // run corresponding command
            }
        }
    }

    clearScreen();
    std::cout << "Terminating program...";
    return 0;
}