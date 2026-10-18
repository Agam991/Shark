#!/usr/bin/env bash
# Shark micro-benchmark: fixed-depth search over a standard position set.
# Deterministic (single-threaded) so nps is comparable across runs / CI.
#
# Usage: bench/bench.sh [engine_path] [depth]
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENGINE="${1:-$ROOT/core/engine}"
DEPTH="${2:-8}"

if [ ! -x "$ENGINE" ]; then
    echo "error: engine not found or not executable: $ENGINE" >&2
    echo "       build it first: cmake --build build -j" >&2
    exit 1
fi

echo "Shark bench  (engine: $ENGINE, depth: $DEPTH)"
printf 'bench %s\nquit\n' "$DEPTH" | "$ENGINE" 2>/dev/null
