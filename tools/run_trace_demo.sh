#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

ELF_PATH="${1:-benchmarks/hello.elf}"
MAX_CYCLES="${2:-2000}"

python3 tools/trace_server.py \
  --host 0.0.0.0 \
  --port 8080 \
  --web-root web \
  -- ./build/mycpu --load "$ELF_PATH" --trace-json stdout --max-cycles "$MAX_CYCLES"
