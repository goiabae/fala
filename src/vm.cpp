#include "vm.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <stack>

#include "lir.hpp"

using std::array;

using lir::Opcode;
using lir::Operand;

namespace vm {

void err(const char* msg) {
	fprintf(stderr, "VM ERROR: %s\n", msg);
	exit(1);
}

#define FETCH(OPND)                                              \
	(((OPND).type == Operand::Type::REG) ? cells[(OPND).reg.index] \
	 : ((OPND).type == Operand::Type::ARR)                         \
	   ? cells[(OPND).arr.start_pointer_reg.index]                 \
	   : (OPND).num)

#define DEREF(OPND) cells[(OPND).reg.index]

#define INDIRECT_LOAD(BASE, OFF) cells[(size_t)(FETCH(BASE) + FETCH(OFF))]

void run(const lir::Chunk& code) {
	array<int64_t, 2048> cells {};

	std::stack<int64_t> stack {};

	constexpr auto read_buffer_cap = 50;
	char read_buffer[read_buffer_cap] {0};

	size_t return_address = 0;

	size_t pc = 0;
	while (pc < code.m_vec.size()) {
		const auto& inst = code.m_vec[pc];
		switch (inst.opcode) {
			case Opcode::PRINTF: {
				size_t i = 0;
				char c;
				while ((c = (char)INDIRECT_LOAD(inst.operands[0], Operand((Number)i)))
				       != 0) {
					printf("%c", c);
					i++;
				}

				break;
			}
			case Opcode::PRINTV: {
				printf("%ld", FETCH(inst.operands[0]));
				break;
			}
			case Opcode::PRINTC: {
				printf("%c", (char)FETCH(inst.operands[0]));
				break;
			}
			case Opcode::READV: {
				if (fgets(read_buffer, read_buffer_cap, stdin) == nullptr)
					err("Couldn't read input");
				const size_t len = strlen(read_buffer);
				read_buffer[len - 1] = '\0';

				char* endptr = nullptr;
				int num = (int)strtol(read_buffer, &endptr, 10);
				if (endptr == read_buffer) err("Could not convert input to integer");

				if (inst.operands[0].type != Operand::Type::REG)
					err("First argument must be a register");

				DEREF(inst.operands[0]) = num;
				break;
			}
			case Opcode::READC: {
				char c = (char)fgetc(stdin);
				DEREF(inst.operands[0]) = (c == EOF) ? -1 : c;
				break;
			}
			case Opcode::MOV: {
				const auto& dest = inst.operands[0];
				const auto& src = inst.operands[1];
				DEREF(dest) = FETCH(src);
				break;
			}
#define BIN_ARITH_OP(OP) \
	DEREF(inst.operands[0]) = FETCH(inst.operands[1]) OP FETCH(inst.operands[2]);
			case Opcode::ADD: BIN_ARITH_OP(+); break;
			case Opcode::SUB: BIN_ARITH_OP(-); break;
			case Opcode::MUL: BIN_ARITH_OP(*); break;
			case Opcode::DIV: BIN_ARITH_OP(/); break;
			case Opcode::MOD: BIN_ARITH_OP(%); break;
			case Opcode::OR: BIN_ARITH_OP(||); break;
			case Opcode::AND: BIN_ARITH_OP(&&); break;
			case Opcode::EQ: BIN_ARITH_OP(==); break;
			case Opcode::DIFF: BIN_ARITH_OP(!=); break;
			case Opcode::LESS: BIN_ARITH_OP(<); break;
			case Opcode::LESS_EQ: BIN_ARITH_OP(<=); break;
			case Opcode::GREATER: BIN_ARITH_OP(>); break;
			case Opcode::GREATER_EQ: BIN_ARITH_OP(>=); break;
			case Opcode::NOT:
				DEREF(inst.operands[0]) = !FETCH(inst.operands[1]);
				break;
			case Opcode::LOAD:
				DEREF(inst.operands[0]) =
					INDIRECT_LOAD(inst.operands[2], inst.operands[1]);
				break;
			case Opcode::STORE:
				INDIRECT_LOAD(inst.operands[2], inst.operands[1]) =
					FETCH(inst.operands[0]);
				break;
			case Opcode::JMP:
				pc = code.label_indexes.at(inst.operands[0].lab.id);
				goto dont_inc;
			case Opcode::JMP_FALSE:
				if (!FETCH(inst.operands[0]))
					pc = code.label_indexes.at(inst.operands[1].lab.id);
				else
					pc++;
				goto dont_inc;
			case Opcode::JMP_TRUE:
				if (FETCH(inst.operands[0]))
					pc = code.label_indexes.at(inst.operands[1].lab.id);
				else
					pc++;
				goto dont_inc;
			case Opcode::PUSH: stack.push(FETCH(inst.operands[0])); break;
			case Opcode::POP:
				DEREF(inst.operands[0]) = stack.top();
				stack.pop();
				break;
			case Opcode::CALL:
				stack.push((int64_t)pc);
				pc = code.label_indexes.at(inst.operands[0].lab.id) - 1;
				break;
			case Opcode::RET: pc = return_address; break;
			case Opcode::FUNC: return_address = (size_t)stack.top(); stack.pop();
		}
		pc++;
	dont_inc:
		(void)0;
	}
}

} // namespace vm
