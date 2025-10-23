#include <gtest/gtest.h>

#include "lir.hpp"
#include "vm.hpp"

TEST(VMTest, move_immediate) {
	auto a = lir::Operand(lir::Register(0).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand(69));

	lir::VM vm {};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[a.as_register().index], 69);
}

TEST(VMTest, move_integer_register) {
	auto a = lir::Operand(lir::Register(0).as_num());
	auto b = lir::Operand(lir::Register(1).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand(69));
	chunk.emit(lir::Opcode::MOV, b, a);

	lir::VM vm {};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[b.as_register().index], 69);
}

TEST(VMTest, arithmetic) {
	auto a = lir::Operand(lir::Register(0).as_num());
	auto b = lir::Operand(lir::Register(1).as_num());
	auto c = lir::Operand(lir::Register(2).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand(3));
	chunk.emit(lir::Opcode::MOV, b, lir::Operand(4));
	chunk.emit(lir::Opcode::MOV, c, lir::Operand(5));
	chunk.emit(lir::Opcode::ADD, a, b, c);
	chunk.emit(lir::Opcode::MUL, c, a, b);

	lir::VM vm {};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[c.as_register().index], 36);
}

TEST(VMTest, store_immediate) {
	auto a = lir::Operand(lir::Register(0).as_addr());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, 1);
	chunk.emit(lir::Opcode::STORE, lir::Operand(69), lir::Operand(0), a);

	lir::VM vm {};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[1], 69);
}

TEST(VMTest, store_immediate_with_offset) {
	auto a = lir::Operand(lir::Register(0).as_addr());
	auto b = lir::Operand(lir::Register(1).as_num());

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, 2);
	chunk.emit(lir::Opcode::MOV, b, 3);
	chunk.emit(lir::Opcode::STORE, lir::Operand(69), b, a);

	lir::VM vm {};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.cells[5], 69);
}

TEST(VMTest, push_immediate) {
	auto a = lir::Operand(69);

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::PUSH, a);

	lir::VM vm {};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(vm.stack.top(), 69);
}
