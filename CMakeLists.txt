cmake_minimum_required(VERSION 3.16)
project(OS_Emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

set(SOURCE_FILES
    src/main.cpp
    src/config/Config.cpp
    src/process/Process.cpp
    src/process/Instruction.cpp
    src/process/InstructionGenerator.cpp
    src/process/ProcessLogBuffer.cpp
    src/scheduler/Scheduler.cpp
    src/utils/Display.cpp
    src/utils/Input.cpp
    src/utils/Reports.cpp
    src/utils/Time.cpp
)

add_executable(OS_Emulator ${SOURCE_FILES}
    include/process/SymbolTable.hpp)

target_include_directories(OS_Emulator PRIVATE
    include
    src
)

target_link_libraries(OS_Emulator PRIVATE Threads::Threads)

target_compile_definitions(OS_Emulator PRIVATE PROJECT_DIR="${CMAKE_SOURCE_DIR}")