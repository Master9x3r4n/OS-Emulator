#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

#include "header/Input.hpp"
#include "header/Display.hpp"
#include "header/Config.hpp"

Config config = {
    numCpu: 24,
    scheduler: "rr", 
    quantumCycles: 5,
    batchProcessFreq: 1,
    minIns: 1000,
    maxIns: 2000,
    delayPerExec: 0
};

int main() {
    bool isProgramRunning = true, isProgramInitialized = false;
    std::vector<std::string> commandInput;

    clearScreen();
    displayConsole();
    while (isProgramRunning)
    {
        commandInput = getInput();
        clearScreen();
        displayConsole();

        if (commandInput[0] == "initialize")
        {
            isProgramInitialized = initializeConfig(config);
        } 
        else if (commandInput[0] == "exit")
        {
            isProgramRunning = false;
        }
        else if (commandInput[0] == "help")
        {
            displayHelp(isProgramInitialized);
        }
        else if (!isProgramInitialized && (commandInput[0] != "clear"))
        {
            std::cout << "Error occured. Initialize program first. Type 'help' for list of valid commands." << "\n";
        } 
        else 
        {
            if (commandInput[0] == "report-util")
            {
                // TODO 1: call same function as `screen -ls` but save output into text file
            }
            else if (commandInput[0] == "scheduler-start")
            {
                // TODO 2: call coresponding thread module to generate dummy processes
            }
            else if (commandInput[0] == "scheduler-stop")
            {
                // TODO 2: call coresponding thread module to stop generating dummy processes
            }
            else if (commandInput[0] == "screen")
            {
                if ((commandInput.size() < 2) || ((commandInput.size() == 2) && (commandInput[1] != "-ls")))
                {
                    displayScreenHelp();
                }
                else if (commandInput[1] == "-ls")
                {
                    // TODO 1: call function to generate current cpu util report
                    std::cout <<"wow you print info\n";
                }
                else if (commandInput[1] == "-s")
                {
                    //TODO 3: call module to create new process then move to process
                    std::cout <<"wow you generate process\n";
                }
                else if (commandInput[1] == "-r")
                {
                    //TODO 3: call module to move to corresponding process
                    std::cout <<"wow you access process\n";
                }
            }
            else if (commandInput[0] != "clear")
            {
                std::cout <<"Unkown command. Type 'help' for list of valid commands." << "\n";
            }
        }
    }
    clearScreen();
    std::cout << "Terminating program...";


    return 0;
}
