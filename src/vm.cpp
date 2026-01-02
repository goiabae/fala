#include "vm.hpp"

#include <iostream>

#include "lir.hpp"

using lir::Operand;

namespace lir {

void err(const char* msg) {
	fprintf(stderr, "VM ERROR: %s\n", msg);
	exit(1);
}

#define INDIRECT_LOAD(BASE, OFF) cells[(size_t)(fetch(BASE) + fetch(OFF))]

void VM::run(const lir::Chunk& code) {
	size_t return_address = 0;

	auto deref = [&](Operand opnd) -> int64_t& {
		if (opnd.type == Operand::Type::REGISTER) {
			return cells[opnd.as_register().index];
		} else if (opnd.type == Operand::Type::ARR) {
			return cells[opnd.as_array().start_pointer_reg.index];
		} else {
			fprintf(stderr, "%s\n", operand_type_repr(opnd.type));
			exit(1);
		}
	};

	auto fetch = [&](Operand opnd) -> int64_t {
		if (opnd.type == Operand::Type::REGISTER) {
			return cells[opnd.as_register().index];
		} else if (opnd.type == Operand::Type::ARR) {
			return cells[opnd.as_array().start_pointer_reg.index];
		} else if (opnd.type == Operand::Type::IMMEDIATE) {
			return opnd.as_number();
		} else if (opnd.type == Operand::Type::NOTHING) {
			return 0;
		} else {
			fprintf(stderr, "%s\n", operand_type_repr(opnd.type));
			exit(1);
		}
	};

	size_t pc = 0;
	while (pc < code.m_vec.size()) {
		const auto& inst = code.m_vec[pc];
		switch (inst.opcode) {
			case Opcode::PRINTF: {
				size_t i = 0;
				char c;
				while ((c = (char)INDIRECT_LOAD(
									inst.operands[0], Operand::make_immediate_integer((int)i)
								))
				       != 0) {
					output << c;
					i++;
				}
				break;
			}
			case Opcode::PRINTV: {
				output << fetch(inst.operands[0]);
				break;
			}
			case Opcode::PRINTC: {
				output << (char)fetch(inst.operands[0]);
				break;
			}
			case Opcode::READV: {
				std::string line {};
				if (not std::getline(input, line)) err("Couldn't read input");
				int num = std::stoi(line);
				if (inst.operands[0].type != Operand::Type::REGISTER)
					err("First argument must be a register");
				deref(inst.operands[0]) = num;
				break;
			}
			case Opcode::READC: {
				char c;
				input >> c;
				deref(inst.operands[0]) = (c == EOF) ? -1 : c;
				break;
			}
			case Opcode::MOV: {
				const auto& dest = inst.operands[0];
				const auto& src = inst.operands[1];
				auto& cell = deref(dest);
				cell = fetch(src);
				break;
			}
#define BIN_ARITH_OP(OP) \
	deref(inst.operands[0]) = fetch(inst.operands[1]) OP fetch(inst.operands[2]);
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
				deref(inst.operands[0]) = !fetch(inst.operands[1]);
				break;
			case Opcode::LOAD:
				deref(inst.operands[0]) =
					INDIRECT_LOAD(inst.operands[2], inst.operands[1]);
				break;
			case Opcode::STORE:
				INDIRECT_LOAD(inst.operands[2], inst.operands[1]) =
					fetch(inst.operands[0]);
				break;
			case Opcode::JMP:
				pc = code.label_indexes.at(inst.operands[0].as_label().id);
				goto dont_inc;
			case Opcode::JMP_FALSE:
				if (!fetch(inst.operands[0]))
					pc = code.label_indexes.at(inst.operands[1].as_label().id);
				else
					pc++;
				goto dont_inc;
			case Opcode::JMP_TRUE:
				if (fetch(inst.operands[0]))
					pc = code.label_indexes.at(inst.operands[1].as_label().id);
				else
					pc++;
				goto dont_inc;
			case Opcode::PUSH: stack.push(fetch(inst.operands[0])); break;
			case Opcode::POP:
				deref(inst.operands[0]) = stack.top();
				stack.pop();
				break;
			case Opcode::CALL:
				stack.push((int64_t)pc);
				pc = code.label_indexes.at(inst.operands[0].as_label().id) - 1;
				break;
			case Opcode::RET: pc = return_address; break;
			case Opcode::FUNC: return_address = (size_t)stack.top(); stack.pop();
		}
		pc++;
	dont_inc:
		(void)0;
	}

	if (should_print_result and code.result_opnd.has_value()) {
		if (auto result = code.result_opnd.value(); code.result_opnd.has_value()) {
			if (result.type == Operand::Type::IMMEDIATE) {
				std::cout << "==> " << fetch(result) << '\n';
			} else if (auto reg = result.as_register();
			           result.type == Operand::Type::REGISTER) {
				if (reg.has_num()) {
					std::cout << "==> " << fetch(result) << '\n';
				} else if (reg.has_addr()) {
					std::cout << "==> 0d" << cells[reg.index] << '\n';
				}
			} else {
				std::cout << "VM ERROR: Couldn't print value" << '\n';
			}
		}
	}
}

} // namespace lir
