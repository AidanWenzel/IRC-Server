cmake_minimum_required(VERSION 3.9)
project(IRC)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 11)

add_executable(IRC main.cpp)

target_link_libraries(IRC Threads::Threads)