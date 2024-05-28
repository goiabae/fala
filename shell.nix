{ pkgs ? import <nixpkgs> {} }:
let
  rev = "0bcbb978795bab0f1a45accc211b8b0e349f1cdb";
  old = import (builtins.fetchTarball {
    url = "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz";
  }) {};
in with pkgs; mkShell {
  nativeBuildInputs = [
    stdenv.cc
    old.bison # 3.0.5
    readline
    cmake
    cppcheck
  ];

  hardeningDisable = [ "fortify" ];
}
