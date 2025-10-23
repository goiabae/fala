{ pkgs ? import <nixpkgs> {} }:
with pkgs; mkShell {
  LD_LIBRARY_PATH = lib.makeLibraryPath [ gcc14Stdenv.cc.cc ];

  nativeBuildInputs = [
    gcc14
    bison
    readline
    (cmake.override { stdenv = gcc14Stdenv; })
    cppcheck
    clang-analyzer

    # The clangd executable provided by the "clang" package uses the unwrapped
    # version of the clang executable, which has no idea where to find the
    # standard library headers and will just report them as missing. Instead,
    # always use the clangd from "clang-tools"
    clang-tools

    gdb
    gtest
  ];

  hardeningDisable = [ "fortify" ];
}
