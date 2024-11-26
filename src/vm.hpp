// Bytecode Virtual Machine

#ifndef VM_HPP
#define VM_HPP

#include "lir.hpp"

namespace vm {

void run(const lir::Chunk&);

}

#endif
