#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <stddef.h>
#include <stdio.h>

#include <string>
#include <utility>
#include <vector>

#include "ast.h"
#include "bytecode.hpp"
#include "env.hpp"
#include "str_pool.h"

using std::pair;
using std::string;
using std::vector;

using bytecode::Chunk;
using bytecode::Operand;

struct Compiler {
	Compiler();
	~Compiler();

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
	size_t back_patch_stack_len {0};
	size_t* back_patch_stack;
	size_t back_patch_len {0};
	size_t* back_patch;

	void push_to_back_patch(size_t idx);
	void back_patch_jumps(Chunk* chunk, Operand dest);

	// used to backpatch the location of the dynamic allocation region start
	Number dyn_alloc_start {2047};

	Env<Operand> env;

	Operand builtin_read(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_write(Chunk* chunk, size_t argc, Operand args[]);
	Operand builtin_array(Chunk* chunk, size_t argc, Operand args[]);
};

#endif
