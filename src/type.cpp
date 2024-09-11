#include "type.hpp"

bool equiv(Type* x, Type* y) { return ((*x) == y) or ((*y) == x); }
