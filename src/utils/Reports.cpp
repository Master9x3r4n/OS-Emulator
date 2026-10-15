#include <utils/Reports.hpp>

#include <cmath>

void GenerateReport(const Scheduler::Snapshot& snapshot, bool printOutput)
{
    // Lambda helper to stream report content to any output stream (std::cout or std::ofstream)
    auto renderReport = [&snapshot](std::ostream& outStream) {
        const int nameWidth      = 14;
        const int timestampWidth = 25;
        const int statusWidth    = 12;

        const uint32_t coresAvailable = snapshot.totalCores - snapshot.coresUsed;

        outStream << std::left
                  << "CPU utilization: " << std::lround(snapshot.utilization) << "%\n"
                  << "Cores used: "      << snapshot.coresUsed                << "\n"
                  << "Cores available: " << coresAvailable                    << "\n\n";

        outStream << "---------------------------------------------------------------------------\n";

        // Running processes (ordered by core)
        outStream << "Running processes:\n";
        for (const auto& p : snapshot.running)
        {
            outStream << std::left << std::setw(nameWidth)      << p.name
                      << std::left << std::setw(timestampWidth) << ("(" + p.startedAt + ")")
                      << std::left << std::setw(statusWidth)    << ("Core: " + std::to_string(p.core))
                      << p.executed << " / " << p.total << "\n";
        }

        // Finished processes (in order of completion)
        outStream << "\nFinished processes:\n";
        for (const auto& p : snapshot.finished)
        {
            outStream << std::left << std::setw(nameWidth)      << p.name
                      << std::left << std::setw(timestampWidth) << ("(" + p.startedAt + ")")
                      << std::left << std::setw(statusWidth)    << "Finished"
                      << p.executed << " / " << p.total << "\n";
        }

        outStream << "---------------------------------------------------------------------------\n";
    };

    // render output to console window
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
            std::cerr << "Error: Unable to open csopesy-log.txt for writing.\n";
        }
    }
}
