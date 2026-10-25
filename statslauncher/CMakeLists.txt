cmake_minimum_required(VERSION 4.0)

add_executable(statslauncher main.cpp)

target_link_libraries(statslauncher PRIVATE compiler-options)
