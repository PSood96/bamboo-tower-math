#!/usr/bin/env bash
# Build + run Bamboo Tower sanity checks (wild-descent shape).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CPP="$ROOT/cpp"
BIN="$CPP/test_bamboo_tower"

echo "== bamboo-tower-math sanity =="
echo "compiling $CPP/test_bamboo_tower.cpp"
c++ -std=c++17 -O2 -Wall -Wextra -pthread -o "$BIN" "$CPP/test_bamboo_tower.cpp"
echo "running $BIN"
"$BIN"
echo "ok"
