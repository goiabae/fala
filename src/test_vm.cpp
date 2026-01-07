#include <gtest/gtest.h>

#include <sstream>

#include "lir.hpp"
#include "vm.hpp"

static lir::Operand make_integer_register(std::size_t index) {
	return lir::Operand(lir::Register(index, lir::Type::make_integer()));
}

TEST(VMTest, move_immediate) {
	auto a = make_integer_register(0);

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(69));

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(std::get<int64_t>(vm.cells[a.as_register().index]), 69);
}

TEST(VMTest, move_integer_register) {
	auto a = make_integer_register(0);
	auto b = make_integer_register(1);

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(69));
	chunk.emit(lir::Opcode::MOV, b, a);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(std::get<int64_t>(vm.cells[b.as_register().index]), 69);
}

TEST(VMTest, arithmetic) {
	auto a = make_integer_register(0);
	auto b = make_integer_register(1);
	auto c = make_integer_register(2);

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

	EXPECT_EQ(std::get<int64_t>(vm.cells[c.as_register().index]), 36);
}

TEST(VMTest, store_immediate) {
	auto a = make_integer_register(0);

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

	EXPECT_EQ(std::get<int64_t>(vm.cells[1]), 69);
}

TEST(VMTest, store_immediate_with_offset) {
	auto a = make_integer_register(0);
	auto b = make_integer_register(1);

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, a, lir::Operand::make_immediate_integer(2));
	chunk.emit(lir::Opcode::MOV, b, lir::Operand::make_immediate_integer(3));
	chunk.emit_store(lir::Operand::make_immediate_integer(69), b, a);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	EXPECT_EQ(std::get<int64_t>(vm.cells[5]), 69);
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

	EXPECT_EQ(std::get<int64_t>(vm.stack.top()), 69);
}

TEST(VMTest, heap_allocation) {
	auto a = lir::Operand::make_immediate_integer(1);
	auto b = lir::Operand::make_immediate_integer(69);
	auto c = lir::Operand::make_immediate_integer(0);

	auto r1 = make_integer_register(1);
	auto r2 = make_integer_register(2);
	auto r3 = make_integer_register(3);
	auto r4 = make_integer_register(4);
	auto r5 = make_integer_register(5);

	lir::Chunk chunk {};
	chunk.emit(lir::Opcode::MOV, r1, a);
	chunk.emit(lir::Opcode::ALLOCA, r2, r1);
	chunk.emit(lir::Opcode::MOV, r3, b);
	chunk.emit(lir::Opcode::MOV, r4, c);
	chunk.emit(lir::Opcode::STOREA, r3, r4, r2);
	chunk.emit(lir::Opcode::LOADA, r5, r4, r2);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	auto integer_value = std::get<int64_t>(vm.cells[5]);
	EXPECT_EQ(integer_value, 69);
}

TEST(VMTest, function_call) {
	// The following:

	// let fun f x = x + 1 in f 3

	// Compiled into:

	//     mov %0, 2047  ; contains address to start of the last allocated region
	//     jump L000
	// L001:
	//     func
	//     pop %2
	//     add %3, %2, 1
	//     push %3
	//     ret
	// L000:
	//     push 3
	//     call L001
	//     pop %4

	auto heap_start = lir::Operand::make_immediate_integer(2047);
	auto imm_one = lir::Operand::make_immediate_integer(1);
	auto imm_three = lir::Operand::make_immediate_integer(3);

	auto l0 = lir::Operand(lir::Label {0});
	auto l1 = lir::Operand(lir::Label {1});

	auto r0 = make_integer_register(0);
	auto r1 = make_integer_register(1);
	auto r2 = make_integer_register(2);
	auto r3 = make_integer_register(3);
	auto r4 = make_integer_register(4);

	lir::Chunk chunk {};

	chunk.emit(lir::Opcode::MOV, r0, heap_start);
	chunk.emit(lir::Opcode::JMP, l0);

	chunk.add_label(l1);
	chunk.emit(lir::Opcode::FUNC);
	chunk.emit(lir::Opcode::POP, r2);
	chunk.emit(lir::Opcode::ADD, r3, r2, imm_one);
	chunk.emit(lir::Opcode::PUSH, r3);
	chunk.emit(lir::Opcode::RET);

	chunk.add_label(l0);
	chunk.emit(lir::Opcode::PUSH, imm_three);
	chunk.emit(lir::Opcode::CALL, l1);
	chunk.emit(lir::Opcode::POP, r4);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	auto integer_value = std::get<int64_t>(vm.cells[4]);
	EXPECT_EQ(integer_value, 4);
}

TEST(VMTest, function_out_parameter) {
	// The following:

	// let fun f (out x) = x = x + 1 in
	// let var y = 3 in do
	//   f y
	//   y
	// end

	// Compiled into:

	//     mov %0, 2047  ; contains address to start of the last allocated region
	//     jump L000
	// L001:
	//     func
	//     pop %7
	//     mov %8, 0
	//     loada %9, %8, %7
	//     mov %10, 1
	//     add %11, %9, %10
	//     storea %11, %8, %7
	//     push %11
	//     ret
	// L000:
	//     mov %1, 1
	//     alloca %2, %1
	//     mov %3, 0
	//     mov %4, 3
	//     storea %4, %3, %2
	//     push %2
	//     call L001
	//     pop %5
	//     loada %6, %3, %2 ; result (y) store in %6

	// Should evaluate to 4

	auto heap_start = lir::Operand::make_immediate_integer(2047);
	auto imm_zero = lir::Operand::make_immediate_integer(0);
	auto imm_one = lir::Operand::make_immediate_integer(1);
	auto imm_three = lir::Operand::make_immediate_integer(3);

	auto l0 = lir::Operand(lir::Label {0});
	auto l1 = lir::Operand(lir::Label {1});

	auto r0 = make_integer_register(0);
	auto r1 = make_integer_register(1);
	auto r2 = make_integer_register(2);
	auto r3 = make_integer_register(3);
	auto r4 = make_integer_register(4);
	auto r5 = make_integer_register(5);
	auto r6 = make_integer_register(6);
	auto r7 = make_integer_register(7);
	auto r8 = make_integer_register(8);
	auto r9 = make_integer_register(9);
	auto r10 = make_integer_register(10);
	auto r11 = make_integer_register(11);

	lir::Chunk chunk {};

	chunk.emit(lir::Opcode::MOV, r0, heap_start);
	chunk.emit(lir::Opcode::JMP, l0);

	chunk.add_label(l1);
	chunk.emit(lir::Opcode::FUNC);
	chunk.emit(lir::Opcode::POP, r7);
	chunk.emit(lir::Opcode::MOV, r8, imm_zero);
	chunk.emit(lir::Opcode::LOADA, r9, r8, r7);
	chunk.emit(lir::Opcode::MOV, r10, imm_one);
	chunk.emit(lir::Opcode::ADD, r11, r9, r10);
	chunk.emit(lir::Opcode::STOREA, r11, r8, r7);
	chunk.emit(lir::Opcode::PUSH, r11);
	chunk.emit(lir::Opcode::RET);

	chunk.add_label(l0);
	chunk.emit(lir::Opcode::MOV, r1, imm_one);
	chunk.emit(lir::Opcode::ALLOCA, r2, r1);
	chunk.emit(lir::Opcode::MOV, r3, imm_zero);
	chunk.emit(lir::Opcode::MOV, r4, imm_three);
	chunk.emit(lir::Opcode::STOREA, r4, r3, r2);
	chunk.emit(lir::Opcode::PUSH, r2);
	chunk.emit(lir::Opcode::CALL, l1);
	chunk.emit(lir::Opcode::POP, r5);
	chunk.emit(lir::Opcode::LOADA, r6, r3, r2);

	std::istringstream input {""};
	std::ostringstream output {};

	lir::VM vm {input, output};
	vm.should_print_result = false;
	vm.run(chunk);

	auto integer_value = std::get<int64_t>(vm.cells[6]);
	EXPECT_EQ(integer_value, 4);
}
