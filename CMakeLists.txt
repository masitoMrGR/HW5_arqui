cmake_minimum_required(VERSION 3.10)
project(rvsim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(rvsim src/main.cpp src/simulator.cpp src/disasm.cpp)
