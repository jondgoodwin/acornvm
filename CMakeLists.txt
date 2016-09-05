cmake_minimum_required(VERSION 2.8)
project(acornvm)
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories("${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/include/acorn" 	"${CMAKE_SOURCE_DIR}/include/avm")

add_library(acornvm SHARED src/avmlib/avm_array.cpp src/avmlib/avm_gc.cpp 
	src/avmlib/avm_memory.cpp src/avmlib/avm_method.cpp src/avmlib/avm_stack.cpp
	src/avmlib/avm_string.cpp src/avmlib/avm_symbol.cpp src/avmlib/avm_table.cpp
	src/avmlib/avm_thread.cpp src/avmlib/avm_value.cpp src/avmlib/avm_vm.cpp
	src/acorn/acn_gen.cpp src/acorn/acn_lexer.cpp src/acorn/acn_main.cpp 
	src/acorn/acn_parser.cpp
	src/core/typ_file.cpp
	src/core/typ_index.cpp
	src/core/typ_integer.cpp
	src/core/typ_list.cpp
	src/core/typ_method.cpp
	src/core/typ_resource.cpp
	src/core/typ_type.cpp
)


