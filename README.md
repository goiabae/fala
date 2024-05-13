# Fala

Toy programming language similar to Lua with a AST-walking interpreter, REPL and Raposeitor 3-address-code assembly compilation.

# Dependencies

- GNU bison (If you're on Windows, use [this](https://github.com/lexxmark/winflexbison))
- C/++ 11 compiler
- CMake

Optional REPL line editting requires GNU `readline`.

# How to build

To build with GNU readline support, use the `WITH_READLINE` option.

``` console
$ cmake -S . -B build
$ cmake --build build
```
