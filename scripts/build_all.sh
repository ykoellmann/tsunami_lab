#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

nix-shell "$PROJECT_DIR/shell.nix" --run "
  set -e
  cd '$PROJECT_DIR'

  echo '=== Formatting Code ==='
  find src/ -name '*.cpp' -o -name '*.h' | xargs clang-format -i

  echo '=== Building (debug) ==='
  scons mode=debug

  echo '=== Running Unit Tests ==='
  ./build/tests

  echo '=== Building (release) ==='
  scons mode=release

  echo '=== Building Doxygen ==='
  cd '$PROJECT_DIR/docs'
  doxygen Doxyfile

  echo '=== Building Sphinx ==='
  cd '$PROJECT_DIR/sphinx'
  make html

  echo '=== All builds finished ==='
"