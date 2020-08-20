cmake_minimum_required(VERSION 3.7)
project(algorithm_C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")
set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -static-libgcc -static-libstdc++ -lwsock32 -lws2_32")
set(SOURCE_FILES main.cpp libmodbus/modbus.c libmodbus/modbus-data.c libmodbus/modbus-rtu.c libmodbus/modbus-tcp.c)

add_executable(algorithm_C ${SOURCE_FILES})