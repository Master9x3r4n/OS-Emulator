# OS-Emulator
Operating System Emulator made using C++ that emulates a Command-Line Interface (CLI) and a Process Scheduler. The shell reference is based on a general Linux/Windows powershell/Windows command line. The process multiplexer is referenced from the Linux `screen` command and its behavior.


## Developers
- Claro, Stephen
- Infante, Gabriel
- Omandac, Karl
- Placer, John


## Features
- Menu Console
- screen command support
- scheduler
- CPU util report
- OS system congifguration
- process simulation with process instructions


## Requirements
- A C++17 compiler (g++, clang, or MSVC)
- CMake 3.16 or newer
- A terminal that understands ANSI escape codes (Windows Terminal or PowerShell 7 on Windows)


## Build
 
From the repo root:
 
```
cmake -S . -B build
cmake --build build
```
 
- Visual Studio: add `--config Release` to the second command.
- MinGW: use `cmake -S . -B build -G "MinGW Makefiles"` for the first command.


## Run
 
| Platform | Command |
|---|---|
| Linux / macOS / MinGW | `./build/OS_Emulator` |
| Windows (Visual Studio) | `build\Release\OS_Emulator.exe` |
 
The path to `config/config.txt` is baked in at build time, so the program can be launched from any directory. `report-util` writes `csopesy-log.txt` to the directory it was launched from.
 

## Commands
 
`initialize` must be run first to access other commands. See `help` for the list of available commands.
 
| Command | Description |
|---|---|
| `initialize` | Read `config/config.txt`, start the scheduler and its CPU worker threads |
| `help` | Display the list of commands |
| `clear` | Clear the console |
| `exit` | Stop the scheduler, join all threads and shut down |
| `screen -ls` | Show CPU utilization, cores used/available, and running and finished processes |
| `report-util` | Same report as `screen -ls`, also saved to `csopesy-log.txt` |
| `screen -s <name>` | Create a new process (not implemented yet) |
| `screen -r <name>` | Attach to a process (not implemented yet) |
| `scheduler-start` | Continuously generate dummy processes (not implemented yet) |
| `scheduler-stop` | Stop generating dummy processes (not implemented yet) |


## Configuration
 
`config/config.txt` is space-separated, one `key value` per line:
 
```
num-cpu 4
scheduler "rr"
quantum-cycles 5
batch-process-freq 1
min-ins 1000
max-ins 2000
delay-per-exec 0
```
 
| Parameter | Description |
|---|---|
| `num-cpu` | Number of CPU cores, 1 to 128 |
| `scheduler` | `"fcfs"` (first-come-first-serve) or `"rr"` (round robin) |
| `quantum-cycles` | Time slice per core for round robin, at least 1. Ignored by FCFS |
| `batch-process-freq` | CPU cycles between generated processes (used by `scheduler-start`, not yet wired) |
| `min-ins`, `max-ins` | Instruction count range per generated process (not yet wired) |
| `delay-per-exec` | Extra CPU cycles of busy-waiting between instructions. 0 means one instruction per cycle |
 
The config is read at when running `initialize`. You may edit the config file after compilation when you rerun this command.


## Project layout
 
```
config/config.txt                      scheduler and process parameters
include/ and src/
  main.cpp                             command loop and dispatch
  config/Config                        config.txt parsing and validation
  process/Process                      process and instruction representation
  scheduler/Scheduler                  scheduler thread, CPU workers, FCFS and RR
  scheduler/SchedulerSettings.hpp      scheduler constants (tick length)
  utils/Display, Input                 banner, help and prompt handling
  utils/Reports                        screen -ls / report-util output
  utils/Time                           MM/DD/YYYY HH:MM:SSAM timestamps
```

