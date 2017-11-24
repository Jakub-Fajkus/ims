cmake_minimum_required(VERSION 3.8)
project(ims)

set(CMAKE_CXX_STANDARD 17)

add_executable(ims main.cc)

include_directories(simlib)

target_link_libraries(ims simlib.a)

