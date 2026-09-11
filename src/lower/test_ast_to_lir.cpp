#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <variant>

#include "ast.hpp"
#include "lir/lir.hpp"
#include "lower/ast_to_lir.hpp"
#include "scope_analyser.hpp"
#include "str_pool.h"
#include "typecheck.hpp"

static bool compare_maps(
	const std::map<size_t, size_t>& a, const std::map<size_t, size_t>& b
);
static bool operator==(const lir::Register& a, const lir::Register& b);
static bool operator==(const lir::Label& a, const lir::Label& b);
static bool operator==(const lir::Immediate& a, const lir::Immediate& b);
static bool operator==(const lir::Operand& a, const lir::Operand& b);
static bool operator==(const lir::Chunk& a, const lir::Chunk& b);

bool compare_maps(
	const std::map<size_t, size_t>& a, const std::map<size_t, size_t>& b
) {
	if (a.size() != b.size()) return false;
	for (const auto& [k, v] : a) {
		if (not b.contains(k)) return false;
		if (v != b.at(k)) return false;
	}
	for (const auto& [k, v] : b) {
		if (not a.contains(k)) return false;
		if (v != a.at(k)) return false;
	}
	return true;
}

bool operator==(const lir::Register& a, const lir::Register& b) {
	if (a.index != b.index) return false;
	return true;
}

bool operator==(const lir::Label& a, const lir::Label& b) {
	return a.id == b.id;
}

bool operator==(const lir::Immediate& a, const lir::Immediate& b) {
	return a.number == b.number;
}

bool operator==(const lir::Operand& a, const lir::Operand& b) {
	if (a.kind != b.kind) {
		throw std::runtime_error("operand types not equal");
	}
	switch (a.kind) {
		case lir::Operand::Kind::NOTHING: return true;
		case lir::Operand::Kind::REGISTER: {
			return a.as_register() == b.as_register();
		}
		case lir::Operand::Kind::LABEL: {
			return a.as_label() == b.as_label();
		}
		case lir::Operand::Kind::IMMEDIATE: {
			return a.as_immediate() == b.as_immediate();
		}
		case lir::Operand::Kind::BYTE_STRING: {
			return a.as_byte_string().bytes == b.as_byte_string().bytes;
		}
	}
	return false;
}

bool operator==(const lir::Chunk& a, const lir::Chunk& b) {
	if (not compare_maps(a.label_indexes, b.label_indexes)) return false;
	if (a.m_vec.size() != b.m_vec.size()) {
		throw std::runtime_error("chunk instruction counts differ");
	}
	for (size_t i = 0; i < a.m_vec.size(); i++) {
		const auto& ai = a.m_vec[i];
		const auto& bi = b.m_vec[i];
		if (ai.opcode != bi.opcode) {
			throw std::runtime_error("instruction opcodes differ");
		}
		const auto opnd_count = lir::opcode_opnd_count(ai.opcode);
		for (size_t j = 0; j < opnd_count; j++) {
			if (ai.operands[j] != bi.operands[j]) {
				throw std::runtime_error("operands not equal");
			}
		}
		// if (ai.comment != bi.comment) return false;
	}
	if (a.result_opnd.has_value() != b.result_opnd.has_value()) {
		throw std::runtime_error("chunks disagree on having result operands");
	}
	if (a.result_opnd.has_value()) {
		return a.result_opnd.value() == b.result_opnd.value();
	} else {
		return true;
	}
}

static lir::Operand make_int_reg(size_t idx) {
	return lir::Operand(lir::Register(idx, lir::Type::UNKNOWN));
}

TEST(LIRTest, emit_binop) {
	lir::Chunk chunk {};
	EXPECT_EQ(chunk.m_vec.size(), 0);
	chunk.emit_binop(
		BinaryOperator::PLUS,
		lir::Operand(lir::Register(1, lir::Type::UNKNOWN)),
		lir::Immediate(1),
		lir::Immediate(1)
	);
	EXPECT_EQ(chunk.m_vec.size(), 1);
	lir::Chunk expected {};
	lir::Instruction i {};
	i.opcode = lir::Opcode::ADD;
	i.operands[0] = lir::Operand(lir::Register(1, lir::Type::UNKNOWN));
	i.operands[1] = lir::Immediate(1);
	i.operands[2] = lir::Immediate(1);
	expected.m_vec.push_back(i);
	EXPECT_TRUE(chunk == expected);
}

TEST(LIRTest, emit_mov) {
	lir::Chunk chunk {};
	EXPECT_EQ(chunk.m_vec.size(), 0);
	chunk.emit_mov(
		lir::Operand(lir::Register(1, lir::Type::UNKNOWN)), lir::Immediate(1)
	);
	EXPECT_EQ(chunk.m_vec.size(), 1);
	lir::Chunk expected {};
	lir::Instruction i {};
	i.opcode = lir::Opcode::MOV;
	i.operands[0] = lir::Operand(lir::Register(1, lir::Type::UNKNOWN));
	i.operands[1] = lir::Immediate(1);
	expected.m_vec.push_back(i);
	EXPECT_TRUE(chunk == expected);
}

TEST(LIRTest, emit_jmp) {
	lir::Chunk chunk {};
	EXPECT_EQ(chunk.m_vec.size(), 0);
	chunk.emit_jmp(lir::Operand(lir::Label(0)));
	EXPECT_EQ(chunk.m_vec.size(), 1);
	lir::Chunk expected {};
	lir::Instruction i {};
	i.opcode = lir::Opcode::JMP;
	i.operands[0] = lir::Operand(lir::Label(0));
	expected.m_vec.push_back(i);
	EXPECT_TRUE(chunk == expected);
}

TEST(LIRTest, add_label) {
	lir::Chunk chunk {};
	chunk.emit_jmp(lir::Operand(lir::Label(69)));
	chunk.add_label(lir::Operand(lir::Label(69)));
	lir::Chunk expected {};
	lir::Instruction i {};
	i.opcode = lir::Opcode::JMP;
	i.operands[0] = lir::Operand(lir::Label(69));
	expected.m_vec.push_back(i);
	expected.label_indexes[69] = 1;
	EXPECT_TRUE(chunk == expected);
}

TEST(ASTToLIRTest, add_integers) {
	StringPool pool {};
	AST ast {};
	{
		const auto _1 = new_number_node(&ast, {}, 1);
		const auto _2 = new_number_node(&ast, {}, 2);
		const auto _3 = new_node(&ast, NodeType::ADD, {_1, _2});
		ast.root_index = _3;
	}
	ScopeAnalyser scop {ast, pool};
	const auto symtab = scop.analyse();
	Typechecker checker {ast, pool, symtab};
	checker.typecheck();
	compiler::Compiler comp {ast, pool, checker, symtab};
	const auto chunk = comp.compile();
	try {
		const char* b =
			"L000:\n"
			"    jump L001\n"
			"L001:\n"
			"    jump L002\n"
			"L002:\n"
			"    jump L003\n"
			"L003:\n"
			"    add %0:int, 1, 2\n"
			"    jump L004\n"
			"L004:\n"
			"    halt\n";
		std::string expected = std::string(b);
		std::stringstream actualss;
		actualss << chunk;
		std::string actual = actualss.str();
		EXPECT_EQ(expected, actual);
	} catch (std::exception& exn) {
		EXPECT_TRUE(false) << exn.what();
	}
}

TEST(ASTToLIRTest, let_expression) {
	StringPool pool {};
	AST ast {};
	{
		(void)"let var x = 3 in x";
		const auto _1 = new_number_node(&ast, {}, 3);
		const auto _2 = new_string_node(&ast, NodeType::ID, {}, pool, "x");
		const auto _3 = new_empty_node(&ast);
		const auto _9 = new_mode_node(&ast, {}, ModeLiteral::VAR);
		const auto _4 = new_node(&ast, NodeType::VAR_DECL, {_9, _2, _3, _1});
		const auto _5 = new_list_node(&ast, NodeType::METALIST);
		const auto _6 = list_append_node(&ast, _5, _4);
		const auto _7 = new_string_node(&ast, NodeType::ID, {}, pool, "x");
		const auto _8 = new_node(&ast, NodeType::LET, {_6, _7});
		ast.root_index = _8;
		// FIXME: how to handle a ast with no source information?
		ast.file_name = "";
		ast.lines = {"blablabla"};
	}
	ScopeAnalyser scop {ast, pool};
	const auto symtab = scop.analyse();
	Typechecker checker {ast, pool, symtab};
	checker.typecheck();
	compiler::Compiler comp {ast, pool, checker, symtab};
	const auto chunk = comp.compile();
	lir::Chunk expected {};
	{
		const auto hp = lir::Immediate(2047);
		const auto r0 = make_int_reg(0);
		const auto r1 = make_int_reg(1);
		const auto l0 = lir::Operand(lir::Label(0));
		const auto c_three = lir::Immediate(3);
		expected
			.emit_mov(r0, hp) //
			.emit_jmp(l0)
			.add_label(l0)
			.emit_mov(r1, c_three);
		expected.result_opnd = r1;
	}
	try {
		const char* b =
			"L000:\n"
			"    jump L005\n"
			"L001:\n"
			"    jump L002\n"
			"L002:\n"
			"    loada %1:???, 0(%0)\n"
			"    jump L004\n"
			"L003:\n"
			"    jump L007\n"
			"L004:\n"
			"    jump L003\n"
			"L005:\n"
			"    jump L006\n"
			"L006:\n"
			"    alloca %0:???, 1\n"
			"    storea 3, 0(%0)\n"
			"    jump L001\n"
			"L007:\n"
			"    halt\n";
		std::string expected = std::string(b);
		std::stringstream actualss;
		actualss << chunk;
		std::string actual = actualss.str();
		EXPECT_EQ(expected, actual);
	} catch (std::exception& exn) {
		EXPECT_TRUE(false) << exn.what();
	}
}
