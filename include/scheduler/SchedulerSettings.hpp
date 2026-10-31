#pragma once

#include <chrono>
#include <cstdint>

// Compile-time knobs for the scheduler module. Kept in one place so the
// temporary homework behavior is easy to switch off later.
namespace SchedulerSettings {

// Wall-clock length of one CPU tick. Without this the scheduler would burn
// through every instruction in microseconds and "screen -ls" would never
// show a process mid-run.
inline constexpr std::chrono::milliseconds kTickDuration{100};

// Temporary homework-only behavior, switched off for the main project.
// kEnablePrintFileLogging: every process writes its PRINT output to
//   logs/<process_name>.txt (file I/O hurts scheduler performance).
// kSeedTestProcesses: create test processes right after "initialize".
// inline constexpr bool kEnablePrintFileLogging = false;
inline constexpr bool kSeedTestProcesses = false;
inline constexpr uint32_t kTestProcessCount = 10;
inline constexpr uint32_t kTestProcessInstructions = 100;

} // namespace SchedulerSettings
