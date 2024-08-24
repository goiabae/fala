// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include "bytecode.hpp"

namespace vm {

void run(const bytecode::Chunk& code);

}

#endif
