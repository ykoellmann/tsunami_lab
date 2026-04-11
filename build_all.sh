#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"

nix-shell "$PROJECT_DIR/shell.nix" --run "
  set -e

  echo '=== Building Doxygen ==='
  cd '$PROJECT_DIR/docs'
  doxygen Doxyfile

  echo '=== Building Sphinx ==='
  cd '$PROJECT_DIR/sphinx'
  make html

  echo '=== Building with SCons ==='
  cd '$PROJECT_DIR'
  scons

  echo '=== All builds finished ==='
"