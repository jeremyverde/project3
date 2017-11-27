cmake_minimum_required(VERSION 3.8)
project(project3)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        manager.cpp
        router.cpp
        project3.h)

add_executable(project3 ${SOURCE_FILES})