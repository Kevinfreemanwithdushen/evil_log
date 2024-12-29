CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
project(argus)
set(SOURCE src)
set(CMAKE_CXX_STANDARD 17)
INCLUDE_DIRECTORIES(src)
INCLUDE_DIRECTORIES(3rd/include/evillog)
INCLUDE_DIRECTORIES(3rd/include/NanoLog)
INCLUDE_DIRECTORIES(3rd/include)
link_directories(${CMAKE_CURRENT_LIST_DIR}/3rd/lib/evillog)
link_directories(${CMAKE_CURRENT_LIST_DIR}/3rd/lib/NanoLog)



file(GLOB_RECURSE SRC_LIST ${SOURCE}/*.cpp)
set_source_files_properties(
  ${SRC_LIST}
  PROPERTIES
  COMPILE_FLAGS "-Wall -Wextra -Wpedantic -g -Wno-unused-parameter"
)
set(CLIENT_LINK_LIB  pthread rt NanoLog evil_log)

add_executable(bench_nano ${SRC_LIST})
target_include_directories(bench_nano SYSTEM PRIVATE 3rd/include/)
TARGET_LINK_LIBRARIES(bench_nano ${CLIENT_LINK_LIB})
target_compile_definitions(bench_nano PUBLIC -DBECH_NANO)


add_executable(bench_spd ${SRC_LIST})
target_include_directories(bench_spd SYSTEM PRIVATE 3rd/include/)
TARGET_LINK_LIBRARIES(bench_spd ${CLIENT_LINK_LIB})
target_compile_definitions(bench_spd PUBLIC -DBECH_SPDLOG)



add_executable(bench_evil ${SRC_LIST})
target_include_directories(bench_evil SYSTEM PRIVATE 3rd/include/)
TARGET_LINK_LIBRARIES(bench_evil ${CLIENT_LINK_LIB})
target_compile_definitions(bench_evil PUBLIC -DBECH_EVIL)
