cmake_minimum_required(VERSION 3.7)
project(ss)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-pthread")

set(SOURCE_FILES main.cpp assembler.cpp assembler.h table.h instruction.cpp instruction.h Relocation.cpp Relocation.h)
add_executable(asm ${SOURCE_FILES})
add_executable(emulator emulator.cpp emulator.h table.h Relocation.h Relocation.cpp)