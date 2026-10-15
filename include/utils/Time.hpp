#pragma once

#include <chrono>
#include <string>

// Formats a time point as "MM/DD/YYYY HH:MM:SSAM" (12-hour clock, local time),
// e.g. "08/06/2024 09:15:22AM". Callers add their own surrounding parentheses.
std::string formatTimestamp(
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now());
