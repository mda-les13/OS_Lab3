cmake_minimum_required(VERSION 3.14)
project(Lab3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(${PROJECT_NAME} Main.cpp)

add_subdirectory(Test)

