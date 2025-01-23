cmake_minimum_required(VERSION 3.1.0)

project(fala)

include(CTest)

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
	PARSER ${CMAKE_CURRENT_SOURCE_DIR}/src/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
	DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.hpp
	COMPILE_FLAGS "-v -Werror")

add_executable(fala
	src/main.cpp
	src/str_pool.cpp
	src/lexer.cpp
	src/ast.cpp
	src/typecheck.cpp
	src/walk.cpp
	src/compiler.cpp
	src/lir.cpp
	src/vm.cpp
	src/type.cpp
	${BISON_PARSER_OUTPUTS})

set_property(TARGET fala PROPERTY C_STANDARD 11)
set_property(TARGET fala PROPERTY CXX_STANDARD 20)
target_include_directories(fala PRIVATE src/)
target_include_directories(fala PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

if (WITH_READLINE)
	target_link_libraries(fala readline history)
endif()

enable_testing()

function(test_example example)
	add_test(
		NAME ${example}
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${example}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

test_example(bf)
test_example(collatz)
test_example(dyn_arr)
test_example(fib2)
test_example(fib)
test_example(glider)
test_example(hello_world)
test_example(if_print)
test_example(let)
test_example(max_int)
test_example(paren_assign)
test_example(pithagoras)
test_example(print_int)
test_example(print_str_int)
test_example(vec_sort)
