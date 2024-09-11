cmake_minimum_required(VERSION 3.1.0)

project(fala)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_CPPCHECK "cppcheck")

if(MSVC)
	add_compile_options(/W4 "$<$<CONFIG:DEBUG>:/Zi;/Od>")
else()
	add_compile_options(-Wall -Wextra -Wpedantic -Wconversion "$<$<CONFIG:DEBUG>:-Og;-ggdb;-pg;-fno-omit-frame-pointer;-fno-inline>")
endif()

option(WITH_READLINE "Enable readline line editting" OFF)

if(WITH_READLINE)
	add_compile_options(-D FALA_WITH_READLINE)
endif()

find_package(BISON REQUIRED)

BISON_TARGET(
	PARSER ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c
	DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h
	COMPILE_FLAGS "-v")

add_executable(fala
	src/main.cpp
	src/str_pool.cpp
	src/lexer.cpp
	src/ast.cpp
	src/typecheck.cpp
	src/walk.cpp
	src/compiler.cpp
	src/bytecode.cpp
	src/vm.cpp
	src/type.cpp
	${BISON_PARSER_OUTPUTS})

set_property(TARGET fala PROPERTY C_STANDARD 11)
set_property(TARGET fala PROPERTY CXX_STANDARD 11)
target_include_directories(fala PRIVATE src/)
target_include_directories(fala PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (WITH_READLINE)
	target_link_libraries(fala readline history)
endif()
