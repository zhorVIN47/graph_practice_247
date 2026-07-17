cmake_minimum_required(VERSION 3.15)
project(graph_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(graph_core main.cpp algorithms.cpp)
target_include_directories(graph_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
  target_compile_options(graph_core PRIVATE /O2 /EHsc /bigobj)
else()
  target_compile_options(graph_core PRIVATE -O2)
endif()
