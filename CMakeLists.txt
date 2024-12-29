CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
project(evil_log)
set(SOURCE src)
set(CMAKE_CXX_STANDARD 17)
INCLUDE_DIRECTORIES(src)

add_compile_options(-Wall -g)

file(GLOB_RECURSE SRC_LIST ${SOURCE}/*.cpp)

set(CLIENT_LINK_LIB pthread rt)

add_library(evil_log ${SRC_LIST})
