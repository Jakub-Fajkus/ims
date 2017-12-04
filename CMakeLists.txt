cmake_minimum_required(VERSION 3.8)
project(ims)

set(CMAKE_CXX_STANDARD 17)

add_executable(ims1 main.cc)
add_executable(ims2 main2.cc)

include_directories(simlib)

target_link_libraries(ims1 simlib.a)
target_link_libraries(ims2 simlib.a)

