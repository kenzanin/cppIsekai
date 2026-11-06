cmake_minimum_required(VERSION 3.20)
project(cppIsekai VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(cppIsekai src/main.cpp)
target_compile_options(cppIsekai PRIVATE -Wall -Wextra)

# ponytail: exe-only scaffold - add a src lib target + tests/ + enable_testing() when the first real module lands
