{ pkgs ? import <nixpkgs> {} }:
with pkgs; mkShell {
  nativeBuildInputs = [
    stdenv.cc
    bison
    readline
    cmake
    cppcheck
    clang-analyzer

    # The clangd executable provided by the "clang" package uses the unwrapped
    # version of the clang executable, which has no idea where to find the
    # standard library headers and will just report them as missing. Instead,
    # always use the clangd from "clang-tools"
    clang-tools

    gdb
  ];

  hardeningDisable = [ "fortify" ];
}
