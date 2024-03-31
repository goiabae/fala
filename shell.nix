{ pkgs ? import <nixpkgs> {} }:
with pkgs; mkShell {
  nativeBuildInputs = [
    stdenv.cc
    bison
    flex
    readline
    cmake
  ];
}
