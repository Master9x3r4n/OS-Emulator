#include <iostream>
using namespace std;

void display_console()
{
    cout << "  ______      _______.  ______  .______   _______      _______.____    ____" << "\n";
    cout << " /      |    /       | /  __  \\ |   _  \\ |   ____|    /       |\\   \\  /   /" << "\n";
    cout << "|  ,----'   |   (----`|  |  |  ||  |_)  ||  |__      |   (----` \\   \\/   /" << "\n";
    cout << "|  |         \\   \\    |  |  |  ||   ___/ |   __|      \\   \\      \\_    _/" << "\n";
    cout << "|  `----..----)   |   |  `--'  ||  |     |  |____ .----)   |       |  |" << "\n";
    cout << " \\______||_______/     \\______/ | _|     |_______||_______/        |__|" << "\n";
    cout << "---------------------------------------------------------------------------\n";
    cout << "Welcome to CSOPESY Emulator!\n";
    cout << "---------------------------------------------------------------------------\n";
}

void display_help(bool isInitialized)
{
    cout << "exit - shut down the OS Emulator" << "\n";
    cout << "help - display list of commands" << "\n";
    cout << "initialize -  initialize the processor configuration of the application" << "\n";

    if (isInitialized)
    {
        cout << "report-util - generate and save CPU util report" << "\n";
        cout << "scheduler-start - continuously generates a batch of dummy processes for the CPU scheduler" << "\n";
        cout << "scheduler-stop - stop generating dummy processes" << "\n";
        cout << "screen -ls - lists all running processes " << "\n";
        cout << "screen -r <process_name> - access an existing process " << "\n";
        cout << "screen -s <process_name> - creates a new process " << "\n";
    }
}

void display_screen_help()
{
    cout << "Invalid usage of sreen command: " << "\n";
    cout << "screen -ls - lists all running processes " << "\n";
    cout << "screen -r <process_name> - access an existing process " << "\n";
    cout << "screen -s <process_name> - creates a new process " << "\n";
}