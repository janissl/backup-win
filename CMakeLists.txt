cmake_minimum_required(VERSION 3.13)
project(backup_win)

set(CMAKE_CXX_STANDARD 14)

add_executable(backup_win main.cpp)
target_link_libraries(backup_win shlwapi.lib)
