#include "vm.hpp"

#include <array>
#include <cstdlib>
#include <cstring>

#include "bytecode.hpp"

using std::array;

using bytecode::Chunk;
using bytecode::Opcode;
using bytecode::Operand;

namespace vm {

void err(const char* msg) {
	fprintf(stderr, "VM ERROR: %s\n", msg);
	exit(1);
}

#define FETCH(OPND)                                                  \
	(((OPND).type == Operand::Type::TMP)   ? t_cells[(OPND).reg.index] \
	 : ((OPND).type == Operand::Type::REG) ? r_cells[(OPND).reg.index] \
	                                       : (OPND).num)

#define DEREF(OPND) \
	(((OPND).type == Operand::Type::TMP) ? t_cells : r_cells)[(OPND).reg.index]

#define INDIRECT_LOAD(BASE, OFF) r_cells[(size_t)(FETCH(BASE) + FETCH(OFF))]

void run(const Chunk& code) {
	array<int64_t, 2048> t_cells {};
	array<int64_t, 2048> r_cells {};

	constexpr auto read_buffer_cap = 50;
	char read_buffer[read_buffer_cap] {0};

	size_t pc = 0;
	while (pc < code.m_vec.size()) {
		const auto& inst = code.m_vec[pc];
		switch (inst.opcode) {
			case Opcode::PRINTF: {
				printf("%s", inst.operands[0].str);
				break;
			}
			case Opcode::PRINTV: {
				printf("%ld", FETCH(inst.operands[0]));
				break;
			}
			case Opcode::READ: {
				if (fgets(read_buffer, read_buffer_cap, stdin) == nullptr)
					err("Couldn't read input");
				const size_t len = strlen(read_buffer);
				read_buffer[len - 1] = '\0';

				char* endptr = nullptr;
				int num = (int)strtol(read_buffer, &endptr, 10);
				if (endptr == read_buffer) err("Could not convert input to integer");

				if (!inst.operands[0].is_register())
					err("First argument must be a register");

				DEREF(inst.operands[0]) = num;
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
		}
		pc++;
	dont_inc:
		(void)0;
	}
}

} // namespace vm
