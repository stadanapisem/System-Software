cmake_minimum_required(VERSION 3.7)
project(ss)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES main.cpp compiler.cpp compiler.h table.h instruction.cpp instruction.h Relocation.cpp Relocation.h)
add_executable(compiler ${SOURCE_FILES})
add_executable(emulator emulator.cpp emulator.h table.h Relocation.h Relocation.cpp)