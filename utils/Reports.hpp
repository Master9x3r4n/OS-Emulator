#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

// todo: generate report as passed parameter
void GenerateReport(bool printOutput = false)
{
    // Lambda helper to stream report content to any output stream (std::cout or std::ofstream)
    auto renderReport = [](std::ostream& os) {
        const int nameWidth      = 14;
        const int timestampWidth = 24;
        const int statusWidth    = 12;
        const int progressWidth  = 10;

        os << std::left
           << std::setw(18) << "CPU Utilization:" << "100%\n"
           << std::setw(18) << "Cores Used:"       << "0\n"
           << std::setw(18) << "Cores Available:"  << "100\n";

        os << "---------------------------------------------------------------------------\n";

        // Running Processes
        os << "Running Processes:\n";
        for (int j = 0; j < 4; j++)
        {
            os << "  "
               << std::left  << std::setw(nameWidth)      << ("process0" + std::to_string(j + 1))
               << std::left  << std::setw(timestampWidth) << "(9/15/2026 9:00:01AM)"
               << std::left  << std::setw(statusWidth)    << "Core: 0"
               << std::right << std::setw(progressWidth)  << "999/1000\n";
        }

        // Finished Processes
        os << "\nFinished Processes:\n";
        for (int j = 0; j < 4; j++)
        {
            os << "  "
               << std::left  << std::setw(nameWidth)      << ("process0" + std::to_string(j + 5))
               << std::left  << std::setw(timestampWidth) << "(9/15/2026 9:00:01AM)"
               << std::left  << std::setw(statusWidth)    << "Finished"
               << std::right << std::setw(progressWidth)  << "999/1000\n";
        }

        os << "---------------------------------------------------------------------------\n";
    };

    // render output to the console window
    renderReport(std::cout);

    if (printOutput)
    {
        std::ofstream outFile("csopesy-log.txt");
        if (outFile.is_open())
        {
            renderReport(outFile);
            std::cout << "Report successfully generated and saved to csopesy-log.txt\n";
        }
        else
        {
            std::cerr << "Error: Unable to open csopesylog.txt for writing.\n";
        }
    }
}