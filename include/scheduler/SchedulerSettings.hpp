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

// TEMPORARY (homework only): every process writes its PRINT output to
// logs/<process_name>.txt. Set to false for the machine project, since the
// file I/O hurts scheduler performance.
inline constexpr bool kEnablePrintFileLogging = true;

// TEMPORARY (homework only): create the test processes right after
// "initialize". Set to false once processes come from scheduler-start.
inline constexpr bool kSeedTestProcesses = true;
inline constexpr uint32_t kTestProcessCount = 10;
inline constexpr uint32_t kTestProcessInstructions = 100;

} // namespace SchedulerSettings
