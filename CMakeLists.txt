cmake_minimum_required(VERSION 3.20)

project(vehicle_telemetry_replay
    VERSION 1.0.0
    DESCRIPTION "Vehicle telemetry replay and sensor calibration analyzer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(telemetry_core
    src/can_frame.cpp
    src/csv.cpp
    src/dbc.cpp
    src/decoder.cpp
    src/replay_engine.cpp
    src/socketcan.cpp)

target_include_directories(telemetry_core PUBLIC include)

if (MSVC)
    target_compile_options(telemetry_core PRIVATE /W4)
else()
    target_compile_options(telemetry_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(telemetry_replay src/main.cpp)
target_link_libraries(telemetry_replay PRIVATE telemetry_core)

add_executable(telemetry_cpp_tests tests/cpp/test_telemetry.cpp)
target_link_libraries(telemetry_cpp_tests PRIVATE telemetry_core)

enable_testing()
add_test(NAME telemetry_cpp_tests COMMAND telemetry_cpp_tests)
