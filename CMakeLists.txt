cmake_minimum_required(VERSION 3.16)
project(OS_Emulator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCE_FILES
    src/main.cpp
    src/config/Config.cpp
    src/utils/Display.cpp
    src/utils/Input.cpp
    src/utils/Reports.cpp
)

add_executable(OS_Emulator ${SOURCE_FILES})

target_include_directories(OS_Emulator PRIVATE
    include
    src
)

target_compile_definitions(OS_Emulator PRIVATE PROJECT_DIR="${CMAKE_SOURCE_DIR}")