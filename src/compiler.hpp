#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <stddef.h>
#include <stdio.h>

#include <stack>
#include <string>
#include <vector>

#include "ast.h"
#include "bytecode.hpp"
#include "env.hpp"
#include "str_pool.h"

using std::stack;
using std::string;
using std::vector;

using bytecode::Chunk;
using bytecode::Operand;

struct Compiler {
	Chunk compile(AST ast, const StringPool& pool);

 private:
	Operand compile(Node node, const StringPool& pool, Chunk* chunk);

	// these are monotonically increasing as the compiler goes on
	size_t label_count {0};
	size_t tmp_count {0};
	size_t reg_count {0};

	Operand make_temporary();
	Operand make_register();
	Operand make_label();

	Operand to_rvalue(Chunk* chunk, Operand opnd);

	bool in_loop {false};
	Operand cnt_lab; // jump to on continue
	Operand brk_lab; // jump to on break

	// these are used to backpatch the destination of MOVs generated when
	// compiling BREAK and CONTINUE nodes
	// back_patch contains the indexes in the chunk of MOVs that need patching,
	// while back_patch_count stores the amount of MOVs for each loop that needs
	// patching. This could also be a stack of vectors
	stack<size_t> back_patch_count;
	stack<size_t> back_patch;

	void push_to_back_patch(size_t idx);
	void back_patch_jumps(Chunk* chunk, Operand dest);

	// used to backpatch the location of the dynamic allocation region start
	Number dyn_alloc_start {2047};

	Env<Operand> env;

	Operand builtin_read_int(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_read_char(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_write_int(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_write_char(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_write_str(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_array(Chunk* chunk, size_t argc, Operand args[]);
};

#endif
