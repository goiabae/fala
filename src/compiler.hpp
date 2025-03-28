#ifndef FALA_COMPILER_HPP
#define FALA_COMPILER_HPP

#include <stddef.h>
#include <stdio.h>

#include <stack>
#include <string>
#include <vector>

#include "ast.hpp"
#include "env.hpp"
#include "lir.hpp"
#include "str_pool.h"

namespace compiler {

using std::stack;
using std::string;
using std::vector;

using lir::Chunk;
using lir::Operand;

struct Result {
	Chunk code;
	Operand opnd;
};

struct Compiler {
	Chunk compile(AST& ast, const StringPool& pool);

	Operand compile(
		AST& ast, NodeIndex node_idx, const StringPool& pool, Chunk* chunk
	);

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

	vector<Chunk> functions;

	Result compile_app(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_if(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_for(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_when(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_while(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_let(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_decl(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_ass(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_str(AST& ast, NodeIndex node_idx, const StringPool& pool);
	Result compile_at(AST& ast, NodeIndex node_idx, const StringPool& pool);
};

} // namespace compiler

#endif
