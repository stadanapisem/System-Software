cmake_minimum_required(VERSION 3.7)
project(code)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp compiler.cpp compiler.h table.h instruction.cpp instruction.h Relocation.cpp Relocation.h)
add_executable(code ${SOURCE_FILES})