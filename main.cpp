#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

using namespace std;

#include "header/Input.h"
#include "header/Display.h"
#include "header/Config.h"

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
    vector<string> commandInput;

    while (isProgramRunning)
    {
        display_console();
        commandInput = get_input();

        if (commandInput[0] == "initialize")
        {
            isProgramInitialized = initialize_config(config);
        } 
        else if (commandInput[0] == "exit")
        {
            isProgramRunning = false;
        }
        else if (commandInput[0] == "help")
        {
            display_help(isProgramInitialized);
        }
        else if (!isProgramInitialized)
        {
            cout << "Error occured. Initialize program first" << "\n";
        } 
        else 
        {
            if (commandInput[0] == "report-util")
            {
                // TODO: call same function as `screen -ls` but save output into text file
            }
            else if (commandInput[0] == "scheduler-start")
            {
                // TODO: call coresponding thread module to generate dummy processes
            }
            else if (commandInput[0] == "scheduler-stop")
            {
                // TODO: call coresponding thread module to stop generating dummy processes
            }
            else if (commandInput[0] == "screen")
            {
                if ((commandInput.size() < 2) || ((commandInput.size() == 2) && (commandInput[1] != "-ls")))
                {
                    display_screen_help();
                }
                else if (commandInput[1] == "-ls")
                {
                    // TODO: call function to generate current cpu util report
                    cout <<"wow you print info\n";
                }
                else if (commandInput[1] == "-s")
                {
                    //TODO: call module to create new process then move to process
                    cout <<"wow you generate process\n";
                }
                else if (commandInput[1] == "-r")
                {
                    //TODO: call module to move to corresponding process
                    cout <<"wow you access process\n";
                }
            }
        }
    }


    return 0;
}
