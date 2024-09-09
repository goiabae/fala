#include "typecheck.hpp"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "ast.h"

Type* typecheck(Typechecker& checker, const Node& node);

static void err(Location loc, const char* msg) {
	fprintf(
		stderr,
		"TYPECHECK ERROR at line %d, from %d to %d: %s\n",
		loc.first_line,
		loc.first_column,
		loc.last_column,
		msg
	);
	exit(1);
}

bool equiv(Type* x, Type* y) {
	// there is always a viable substitution between two type variables
	if (dynamic_cast<TypeVar*>(x) != nullptr || dynamic_cast<TypeVar*>(y) != nullptr) {
		return true;
	}

	if (dynamic_cast<Concrete*>(x) != nullptr) {
		auto cx = dynamic_cast<Concrete*>(x);
		if (dynamic_cast<Concrete*>(y) != nullptr) {
			auto cy = dynamic_cast<Concrete*>(y);
			return cx->id == cy->id;
		} else {
			return false;
		}
	}

	if (dynamic_cast<Concrete*>(y) != nullptr) {
		return false;
	}

	if (dynamic_cast<Slice*>(x) != nullptr && dynamic_cast<Slice*>(y) != nullptr) {
		auto sx = dynamic_cast<Slice*>(x);
		auto sy = dynamic_cast<Slice*>(y);
		return equiv(sx->element, sy->element);
	}

	if (dynamic_cast<Poly*>(x) != nullptr && dynamic_cast<Poly*>(y) != nullptr) {
		auto px = dynamic_cast<Poly*>(x);
		auto py = dynamic_cast<Poly*>(y);
		return equiv(px->typ, py->typ);
	}

	if (dynamic_cast<Unique*>(x) != nullptr && dynamic_cast<Unique*>(y) != nullptr) {
		auto ux = dynamic_cast<Unique*>(x);
		auto uy = dynamic_cast<Unique*>(y);
		return ux->id == uy->id;
	}

	if (dynamic_cast<Record*>(x) != nullptr && dynamic_cast<Record*>(y) != nullptr) {
		auto rx = dynamic_cast<Record*>(x);
		auto ry = dynamic_cast<Record*>(y);

		if (rx->fields.size() != ry->fields.size()) return false;

		size_t field_count = rx->fields.size();
		for (size_t i = 0; i < field_count; i++) {
			auto fx = rx->fields[i];
			auto fy = ry->fields[i];
			if (fx.first.idx != fy.first.idx) return false;
			if (!equiv(fx.second, fy.second)) return false;
		}

		return true;
	}

	return false;
}

bool typecheck(const AST& ast) {
	Typechecker checker {};
	typecheck(checker, ast.root);
	return true;
}

Type* typecheck(Typechecker& checker, const Node& node) {
	switch (node.type) {
		case AST_EMPTY: return checker.get_nil();
		case AST_APP: return checker.get_nil(); // FIXME
		case AST_NUM: return checker.get_numeric();
		case AST_BLK: {
			for (size_t i = 0; i < node.branch.children_count - 1; i++)
				typecheck(checker, node.branch.children[i]);
			return typecheck(
				checker, node.branch.children[node.branch.children_count - 1]
			);
		}
		case AST_IF: {
			const auto t = typecheck(checker, node.branch.children[1]);
			const auto f = typecheck(checker, node.branch.children[2]);
			if (!equiv(t, f))
				err(
					node.branch.children[1].loc,
					"If expression has \"then\" and \"else\" branches with different "
					"types"
				);
			return t;
		}
		case AST_WHEN: {
			auto tc = typecheck(checker, node.branch.children[0]);
			if (!equiv(tc, checker.get_bool()))
				err(
					node.branch.children[0].loc,
					"Condition expression of when expression is not of type boolean"
				);
			typecheck(checker, node.branch.children[1]);
			return checker.get_nil();
		}
		case AST_FOR: {
			typecheck(checker, node.branch.children[0]);
			typecheck(checker, node.branch.children[1]);
			if (node.branch.children[2].type != AST_EMPTY)
				typecheck(checker, node.branch.children[2]);
			return typecheck(checker, node.branch.children[3]);
		}
		case AST_WHILE: {
			typecheck(checker, node.branch.children[0]);
			return typecheck(checker, node.branch.children[1]);
		}
		case AST_BREAK:
		case AST_CONTINUE: return typecheck(checker, node.branch.children[0]);
		case AST_ASS: {
			typecheck(checker, node.branch.children[0]);
			return typecheck(checker, node.branch.children[1]);
		}
		case AST_EQ: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			if (!equiv(left, right))
				err(
					node.branch.children[0].loc,
					"Equality comparison of values of different types is always false"
				);
			return checker.get_bool();
		}
		case AST_OR:
		case AST_AND: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			if (!(equiv(left, checker.get_bool()) && equiv(right, checker.get_bool())
			    ))
				err(
					node.branch.children[0].loc,
					"Logical combinator arguments must be of boolean type"
				);
			return checker.get_bool();
		}
		case AST_GTN:
		case AST_LTN:
		case AST_GTE:
		case AST_LTE: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			if (!(equiv(left, checker.get_numeric())
			      && equiv(right, checker.get_numeric())))
				err(
					node.branch.children[0].loc,
					"Comparison operator arguments must be of numeric type"
				);
			return checker.get_bool();
		}
		case AST_ADD:
		case AST_SUB:
		case AST_MUL:
		case AST_DIV:
		case AST_MOD: {
			const auto left = typecheck(checker, node.branch.children[0]);
			const auto right = typecheck(checker, node.branch.children[1]);
			const auto num = checker.get_numeric();
			if (!(equiv(left, num) && equiv(right, num)))
				err(
					node.branch.children[0].loc,
					"Arithmetic operator arguments must be of numeric type"
				);
			return checker.get_numeric();
		}
		case AST_AT:
			typecheck(checker, node.branch.children[0]);
			return checker.get_numeric();
		case AST_NOT:
			typecheck(checker, node.branch.children[0]);
			return checker.get_bool();
		case AST_ID: return checker.make_var();
		case AST_STR: return checker.make_slice(checker.get_character());
		case AST_DECL: {
			if (node.branch.children_count == 3)
				typecheck(checker, node.branch.children[2]);
			else if (node.branch.children_count == 4)
				typecheck(checker, node.branch.children[3]);
			else
				assert(false);
			return checker.get_nil();
		}
		case AST_NIL: return checker.get_nil();
		case AST_TRUE: return checker.get_bool();
		case AST_LET: {
			const Node& decls = node.branch.children[0];
			const Node& exp = node.branch.children[1];

			for (size_t i = 0; i < decls.branch.children_count; i++)
				typecheck(checker, decls.branch.children[i]);
			return typecheck(checker, exp);
		}
		case AST_CHAR: return checker.get_numeric();
		case AST_PATH: return typecheck(checker, node.branch.children[0]);
		case AST_PRIMITIVE_TYPE: assert(false && "TODO");
	}

	assert(false && "unreachable");
}
