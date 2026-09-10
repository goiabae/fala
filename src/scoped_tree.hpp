#ifndef SCOPED_TREE_HPP
#define SCOPED_TREE_HPP

#include <cassert>

#include "ast.hpp"
#include "str_pool.h"
#include "symbol_table.hpp"

static void ast_print_scoped(AST*, const StringPool&, const SymbolTable&);

static void print_spaces(FILE* fd, unsigned int count) {
	for (size_t i = 0; i < count; i++) fprintf(fd, " ");
}

static void ast_node_print_scoped(
	AST* ast, const StringPool& pool, const SymbolTable& symtab,
	NodeIndex node_idx, unsigned int space
) {
	const auto fd = stderr;
	const auto& node = ast->at(node_idx);

	print_spaces(fd, space);
	fprintf(fd, "{\n");
	print_spaces(fd, space + 2);
	fprintf(fd, "type = %s\n", node_type_repr(node.type));
	print_spaces(fd, space + 2);
	fprintf(fd, "index = %d\n", node_idx.index);
	print_spaces(fd, space + 2);
	fprintf(fd, "loc = %d\n", node.loc.begin.byte_offset);

	if (node.type == NodeType::NUM) {
		print_spaces(fd, space + 2);
		fprintf(fd, "num = %d\n", node.num);
	} else if (node.type == NodeType::TRUE) {
		print_spaces(fd, space + 2);
		fprintf(fd, "literal = true\n");
	} else if (node.type == NodeType::FALSE) {
		print_spaces(fd, space + 2);
		fprintf(fd, "literal = false\n");
	} else if (node.type == NodeType::NIL) {
	} else if (node.type == NodeType::ID) {
		print_spaces(fd, space + 2);
		fprintf(fd, "id = %s\n", pool.find(node.str_id));
	} else if (node.type == NodeType::GUID) {
		const auto s = symtab.declaration_scope_of(node.guid).value();
		const auto f = symtab.function_of(s);
		print_spaces(fd, space + 2);
		fprintf(fd, "guid = %u\n", node.guid.index);
		print_spaces(fd, space + 2);
		fprintf(fd, "declaration_scope = %u\n", s.index);
		if (f.has_value()) {
			print_spaces(fd, space + 2);
			fprintf(fd, "declaration_function = %u\n", f.value().index);
		}
	} else if (node.type == NodeType::STR) {
		print_spaces(fd, space + 2);
		fprintf(fd, "str = \"");
		for (char* it = (char*)pool.find(node.str_id); *it != '\0'; it++) {
			if (*it == '\n')
				fprintf(fd, "\\n");
			else
				fprintf(fd, "%c", *it);
		}
		fprintf(fd, "\"\n");
	} else if (node.type == NodeType::CHAR) {
		print_spaces(fd, space + 2);
		fprintf(fd, "char = '%c'\n", node.character);
	} else if (node.type == NodeType::EMPTY) {
	} else if (node.type == NodeType::MODE) {
		print_spaces(fd, space + 2);
		fprintf(fd, "mode = %s\n", mode_repr(node.mode));
	} else if (node.type == NodeType::METALIST) {
		print_spaces(fd, space + 2);
		fprintf(fd, "%zu [\n", node.branch.children_count);

		for (auto idx : node) {
			ast_node_print_scoped(ast, pool, symtab, idx, space + 2 + 2);
		}

		print_spaces(fd, space + 2);
		fprintf(fd, "]\n");
	} else {
		assert(is_branch_node(node.type));
		print_spaces(fd, space + 2);
		fprintf(fd, "children = %zu [\n", node.branch.children_count);

		for (auto idx : node) {
			ast_node_print_scoped(ast, pool, symtab, idx, space + 2 + 2);
		}

		print_spaces(fd, space + 2);
		fprintf(fd, "]\n");
	}

	print_spaces(fd, space);
	fprintf(fd, "}\n");
}

void ast_print_scoped(
	AST* ast, const StringPool& pool, const SymbolTable& symtab
) {
	ast_node_print_scoped(ast, pool, symtab, ast->root_index, 0);
}

#endif
