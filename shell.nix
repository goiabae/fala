{ pkgs ? import <nixpkgs> {} }:
with pkgs; mkShell {
  nativeBuildInputs = [
    stdenv.cc
    bison
    readline
    cmake
    cppcheck
    clang-analyzer
    clang-tools
    gdb
  ];

  hardeningDisable = [ "fortify" ];
}
