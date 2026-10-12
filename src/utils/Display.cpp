#include <utils/Display.hpp>

void displayConsole()
{
    std::cout << "  ______      _______.  ______  .______   _______      _______.____    ____" << "\n"
              << " /      |    /       | /  __  \\ |   _  \\ |   ____|    /       |\\   \\  /   /" << "\n"
              << "|  ,----'   |   (----`|  |  |  ||  |_)  ||  |__      |   (----` \\   \\/   /" << "\n"
              << "|  |         \\   \\    |  |  |  ||   ___/ |   __|      \\   \\      \\_    _/" << "\n"
              << "|  `----..----)   |   |  `--'  ||  |     |  |____ .----)   |       |  |" << "\n"
              << " \\______||_______/     \\______/ | _|     |_______||_______/        |__|" << "\n";
    std::cout << "---------------------------------------------------------------------------\n";
    std::cout << "Welcome to CSOPESY Emulator!\n\n";
    std::cout << "Developers: \n"
              << "   Claro, Stephen\n"
              << "   Infante, Gabriel\n"
              << "   Omandac, Karl\n"
              << "   Placer, John\n\n";
    std::cout << "Version Date: v9.15.26\n";
    std::cout << "---------------------------------------------------------------------------\n";
}

void displayHelp(bool isInitialized)
{
    const int colWidth = 28;

    std::cout << "USAGE:\n"
              << "  <command> [options]\n\n"
              << "CORE COMMANDS:\n"
              << "  " << std::left << std::setw(colWidth) << "clear" << "Clear the console content\n"
              << "  " << std::left << std::setw(colWidth) << "exit" << "Shut down the OS Emulator\n"
              << "  " << std::left << std::setw(colWidth) << "help" << "Display list of commands\n"
              << "  " << std::left << std::setw(colWidth) << "initialize" << "Initialize application processor configuration\n";

    if (isInitialized)
    {
        std::cout << "\nPROCESS MANAGEMENT:\n"
                  << "  " << std::left << std::setw(colWidth) << "report-util" << "Generate and save CPU utilization report\n"
                  << "  " << std::left << std::setw(colWidth) << "scheduler-start" << "Continuously generate dummy processes for CPU scheduler\n"
                  << "  " << std::left << std::setw(colWidth) << "scheduler-stop" << "Stop generating dummy processes\n"
                  << "  " << std::left << std::setw(colWidth) << "screen -ls" << "List all running processes\n"
                  << "  " << std::left << std::setw(colWidth) << "screen -r <process_name>" << "Access an existing process\n"
                  << "  " << std::left << std::setw(colWidth) << "screen -s <process_name>" << "Create a new process\n";
    }
    std::cout << "\n";
}

void displayScreenHelp()
{
    const int colWidth = 24;

    std::cerr << "Error: Invalid usage of 'screen' command.\n\n"
              << "USAGE:\n"
              << "  screen <flag> [process_name]\n\n"
              << "FLAGS:\n"
              << "  " << std::left << std::setw(colWidth) << "-ls" << "List all running processes\n"
              << "  " << std::left << std::setw(colWidth) << "-r <process_name>" << "Access an existing process\n"
              << "  " << std::left << std::setw(colWidth) << "-s <process_name>" << "Create a new process\n\n";
}

void clearScreen()
{
    std::cout << "\n\033[H\033[J";
}