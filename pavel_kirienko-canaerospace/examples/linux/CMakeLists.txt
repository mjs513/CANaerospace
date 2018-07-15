cmake_minimum_required(VERSION 2.6)

project(canaerospace_linux_example)

set(CMAKE_C_FLAGS "-std=c99 -g3 -Wall -Wextra -pedantic")

add_executable(canaerospace_linux_example main.c canaerospace_linux.c)

find_library(CANAEROSPACE_LIB canaerospace)
find_library(CANAEROSPACE_SOCKETCAN_LIB canaerospace_socketcan)
target_link_libraries(canaerospace_linux_example ${CANAEROSPACE_LIB} ${CANAEROSPACE_SOCKETCAN_LIB})
