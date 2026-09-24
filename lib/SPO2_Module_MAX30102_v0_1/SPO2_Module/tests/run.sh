#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT
g++ -std=c++11 -Wall -Wextra -Wno-unused-variable -fsanitize=address,undefined -fno-omit-frame-pointer -g -Ilib/ResearchSpO2/src tests/test.cpp lib/ResearchSpO2/src/ResearchSpO2.cpp lib/ResearchSpO2/src/MaximCore.cpp -o "$build_dir/test"
"$build_dir/test"
