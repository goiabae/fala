#include <gtest/gtest.h>

#include <sstream>

#include "lir.hpp"
#include "vm.hpp"

TEST(VMTest, move_immediate) {
	auto a = lir::Operand(lir::Register(0).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(69));

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[a.as_register().index], 69);
}

TEST(VMTest, move_integer_register) {
	auto a = lir::Operand(lir::Register(0).as_num());
	auto b = lir::Operand(lir::Register(1).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(69));
	chunk.emit(lir::Opcode::MOV, b, a);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[b.as_register().index], 69);
}

TEST(VMTest, arithmetic) {
	auto a = lir::Operand(lir::Register(0).as_num());
	auto b = lir::Operand(lir::Register(1).as_num());
	auto c = lir::Operand(lir::Register(2).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(3));
	chunk.emit(lir::Opcode::MOV, b, lir::Operand::make_immediate_integer(4));
	chunk.emit(lir::Opcode::MOV, c, lir::Operand::make_immediate_integer(5));
	chunk.emit(lir::Opcode::ADD, a, b, c);
	chunk.emit(lir::Opcode::MUL, c, a, b);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[c.as_register().index], 36);
}

TEST(VMTest, store_immediate) {
	auto a = lir::Operand(lir::Register(0).as_addr());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(1));
	chunk.emit_store(
		lir::Operand::make_immediate_integer(69),
		lir::Operand::make_immediate_integer(0),
		a
	);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[1], 69);
}

TEST(VMTest, store_immediate_with_offset) {
	auto a = lir::Operand(lir::Register(0).as_addr());
	auto b = lir::Operand(lir::Register(1).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(2));
	chunk.emit(lir::Opcode::MOV, b, lir::Operand::make_immediate_integer(3));
	chunk.emit_store(lir::Operand::make_immediate_integer(69), b, a);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[5], 69);
}

TEST(VMTest, push_immediate) {
	auto a = lir::Operand::make_immediate_integer(69);

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::PUSH, a);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.stack.top(), 69);
}
