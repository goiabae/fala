# Fala

Toy programming language similar to Lua with a AST-walking interpreter, REPL and Raposeitor 3-address-code assembly compilation.

# Dependencies

- GNU bison
- GNU flex
- C11 compiler

Optional REPL line editting requires GNU `readline`.

# How to build

Change the flags you want in the `makefile` and:

``` console
$ make all
```

Or with nix shell:

``` console
$ nix-shell --run 'make all'
```
