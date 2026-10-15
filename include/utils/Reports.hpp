#pragma once

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

#include <scheduler/Scheduler.hpp>

// Prints the CPU utilization report ("screen -ls"). When printOutput is true
// it also saves the same report to csopesy-log.txt ("report-util").
void GenerateReport(const Scheduler::Snapshot& snapshot, bool printOutput = false);
