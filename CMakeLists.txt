cmake_minimum_required(VERSION 3.26)
project(SimpleFlowField C)

set(CMAKE_C_STANDARD 23)

add_subdirectory(vendor)

add_executable(SimpleFlowField main.c)
target_link_libraries(SimpleFlowField raylib)