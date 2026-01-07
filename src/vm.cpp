#include "vm.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>

#include "lir.hpp"

using lir::Operand;

namespace lir {

void err(const char* msg) {
	fprintf(stderr, "VM ERROR: %s\n", msg);
	exit(1);
}

void VM::run(const lir::Chunk& code) {
	size_t return_address = 0;

	auto deref = [&](Operand opnd) -> int64_t& {
		if (opnd.type == Operand::Type::REGISTER) {
			return std::get<int64_t>(cells[opnd.as_register().index]);
		} else {
			fprintf(stderr, "%s\n", operand_type_repr(opnd.type));
			exit(1);
		}
	};

	auto fetch = [&](Operand opnd) -> int64_t {
		if (opnd.type == Operand::Type::REGISTER) {
			return std::get<int64_t>(cells[opnd.as_register().index]);
		} else if (opnd.type == Operand::Type::IMMEDIATE) {
			return opnd.as_immediate().number;
		} else if (opnd.type == Operand::Type::NOTHING) {
			return 0;
		} else {
			fprintf(stderr, "%s\n", operand_type_repr(opnd.type));
			exit(1);
		}
	};

	auto indirect_load = [&](Operand base, Operand off) -> int64_t& {
		return std::get<int64_t>(cells[(size_t)(fetch(base) + fetch(off))]);
	};

	size_t pc = 0;
	while (pc < code.m_vec.size()) {
		const auto& inst = code.m_vec[pc];
		switch (inst.opcode) {
			case Opcode::PRINTF: {
				size_t i = 0;
				char c;
				while ((c = (char)indirect_load(
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
					indirect_load(inst.operands[2], inst.operands[1]);
				break;
			case Opcode::STORE:
				indirect_load(inst.operands[2], inst.operands[1]) =
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
			case Opcode::PUSH: {
				if (inst.operands[0].type == Operand::Type::IMMEDIATE) {
					stack.push(Value(inst.operands[0].as_immediate().number));
				} else if (inst.operands[0].type == Operand::Type::REGISTER) {
					stack.push(cells[inst.operands[0].as_register().index]);
				} else {
					throw std::runtime_error("can't push value");
				}
				break;
			}
			case Opcode::POP: {
				assert(inst.operands[0].type == Operand::Type::REGISTER);
				auto& cell = cells[inst.operands[0].as_register().index];
				cell = stack.top();
				stack.pop();
				break;
			}
			case Opcode::CALL:
				stack.push(Value((int64_t)pc));
				pc = code.label_indexes.at(inst.operands[0].as_label().id) - 1;
				break;
			case Opcode::RET: pc = return_address; break;
			case Opcode::FUNC:
				return_address = (size_t)std::get<int64_t>(stack.top());
				stack.pop();
				break;
			case Opcode::ALLOCA: {
				auto result_register = inst.operands[0];
				auto size_register = inst.operands[1];
				assert(result_register.type == Operand::Type::REGISTER);
				assert(size_register.type == Operand::Type::REGISTER);
				auto size_integer = fetch(size_register);
				assert(size_integer > 0);
				auto register_index = result_register.as_register().index;
				auto& cell = cells[register_index];
				cell = Value(new Value[(size_t)size_integer]);
				break;
			}
			case Opcode::STOREA: {
				auto value_register = inst.operands[0];
				auto offset_register = inst.operands[1];
				auto pointer_register = inst.operands[2].as_register();
				assert(value_register.type == Operand::Type::REGISTER);
				assert(offset_register.type == Operand::Type::REGISTER);
				auto value = fetch(value_register);
				auto offset = fetch(offset_register);
				auto pointer = std::get<Value*>(cells[pointer_register.index]);
				auto& cell = pointer[offset];
				cell = Value(value);
				break;
			}
			case Opcode::LOADA: {
				auto result_register = inst.operands[0];
				auto offset_register = inst.operands[1];
				auto pointer_register = inst.operands[2].as_register();
				assert(result_register.type == Operand::Type::REGISTER);
				assert(offset_register.type == Operand::Type::REGISTER);
				auto offset = fetch(offset_register);
				auto pointer = std::get<Value*>(cells[pointer_register.index]);
				auto value = pointer[offset];
				auto& cell = cells[result_register.as_register().index];
				cell = value;
				break;
			}
		}
		pc++;
	dont_inc:
		(void)0;
	}

	if (should_print_result and code.result_opnd.has_value()) {
		auto result = code.result_opnd.value();
		if (result.type == Operand::Type::IMMEDIATE) {
			std::cout << "==> " << fetch(result) << '\n';
		} else if (auto reg = result.as_register();
		           result.type == Operand::Type::REGISTER) {
			if (not reg.is_lvalue_pointer) {
				std::cout << "==> " << fetch(result) << '\n';
			} else {
				std::cout << "==> 0d" << std::get<int64_t>(cells[reg.index]) << '\n';
			}
		} else {
			std::cout << "VM ERROR: Couldn't print value" << '\n';
		}
	}
}

} // namespace lir
