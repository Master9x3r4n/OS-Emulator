#include <iostream>

void displayConsole()
{
    std::cout << "  ______      _______.  ______  .______   _______      _______.____    ____" << "\n";
    std::cout << " /      |    /       | /  __  \\ |   _  \\ |   ____|    /       |\\   \\  /   /" << "\n";
    std::cout << "|  ,----'   |   (----`|  |  |  ||  |_)  ||  |__      |   (----` \\   \\/   /" << "\n";
    std::cout << "|  |         \\   \\    |  |  |  ||   ___/ |   __|      \\   \\      \\_    _/" << "\n";
    std::cout << "|  `----..----)   |   |  `--'  ||  |     |  |____ .----)   |       |  |" << "\n";
    std::cout << " \\______||_______/     \\______/ | _|     |_______||_______/        |__|" << "\n";
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << "Welcome to CSOPESY Emulator!\n";
    std::cout << "---------------------------------------------------------------------------\n";
}

void displayHelp(bool isInitialized)
{
    std::cout << "clear - clear the console menu content" << "\n";
    std::cout << "exit - shut down the OS Emulator" << "\n";
    std::cout << "help - display list of commands" << "\n";
    std::cout << "initialize -  initialize the processor configuration of the application" << "\n";

    if (isInitialized)
    {
        std::cout << "report-util - generate and save CPU util report" << "\n";
        std::cout << "scheduler-start - continuously generates a batch of dummy processes for the CPU scheduler" << "\n";
        std::cout << "scheduler-stop - stop generating dummy processes" << "\n";
        std::cout << "screen -ls - lists all running processes " << "\n";
        std::cout << "screen -r <process_name> - access an existing process " << "\n";
        std::cout << "screen -s <process_name> - creates a new process " << "\n";
    }
}

void displayScreenHelp()
{
    std::cout << "Error. Invalid usage of screen command." << "\n";
    std::cout << "screen -ls - lists all running processes " << "\n";
    std::cout << "screen -r <process_name> - access an existing process " << "\n";
    std::cout << "screen -s <process_name> - creates a new process " << "\n";
}

void clearScreen()
{
    std::cout << "\033[H\033[J";
}