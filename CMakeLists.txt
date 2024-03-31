cmake_minimum_required(VERSION 3.22.2)

project(fala)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

if(WIN32)
	add_compile_options(/W4)
	set(CMAKE_C_FLAGS_DEBUG "/Zi /Od")
else()
	add_compile_options(-Wall -Wextra -Wpedantic -Wconversion)
	set(CMAKE_C_FLAGS_DEBUG "-ggdb -O0")
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

add_executable(fala src/main.c src/lexer.c src/ast.c src/eval.c src/compiler.c src/env.c ${BISON_PARSER_OUTPUTS})
set_property(TARGET fala PROPERTY C_STANDARD 11)
target_include_directories(fala PRIVATE src/)
target_include_directories(fala PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (WITH_READLINE)
	target_link_libraries(fala readline history)
endif()
