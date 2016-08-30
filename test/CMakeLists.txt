cmake_minimum_required(VERSION 2.8)
project(testavm)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR})

include_directories("${CMAKE_SOURCE_DIR}/../include" "${CMAKE_SOURCE_DIR}/../include/acorn" 	"${CMAKE_SOURCE_DIR}/../include/avm")

add_executable(testavm testavm.cpp testcapi.cpp testgen.cpp testtype.cpp)
target_link_libraries(testavm ${CMAKE_SOURCE_DIR}/../bin/libacornvm.so)


