#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

ELF_PATH="${1:-benchmarks/hello.elf}"
MAX_CYCLES="${2:-2000}"
TRACE_HOST="${TRACE_HOST:-0.0.0.0}"
TRACE_PORT="${TRACE_PORT:-8080}"

is_port_free() {
  python3 - "$1" <<'PY'
import socket
import sys

port = int(sys.argv[1])
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
  s.bind(("127.0.0.1", port))
except OSError:
  sys.exit(1)
finally:
  s.close()
sys.exit(0)
PY
}

if [[ ! -x ./build/mycpu ]]; then
  echo "[run_trace_demo] missing ./build/mycpu, please build first" >&2
  exit 2
fi

if [[ ! -f "$ELF_PATH" ]]; then
  echo "[run_trace_demo] ELF not found: $ELF_PATH" >&2
  exit 2
fi

# Demo mode expects a single trace server. Clear stale processes to avoid binding
# a new child run to a different server instance with an empty buffer.
pkill -f "python3 tools/trace_server.py" 2>/dev/null || true

if ! is_port_free "$TRACE_PORT"; then
  for candidate in 18080 18081 18082 28080; do
    if is_port_free "$candidate"; then
      echo "[run_trace_demo] port ${TRACE_PORT} is busy, fallback to ${candidate}"
      TRACE_PORT="$candidate"
      break
    fi
  done
fi

if ! is_port_free "$TRACE_PORT"; then
  echo "[run_trace_demo] no free port found, set TRACE_PORT manually" >&2
  exit 3
fi

echo "[run_trace_demo] open http://127.0.0.1:${TRACE_PORT}"
echo "[run_trace_demo] elf=${ELF_PATH} max_cycles=${MAX_CYCLES}"

python3 tools/trace_server.py \
  --host "${TRACE_HOST}" \
  --port "${TRACE_PORT}" \
  --web-root web \
  -- ./build/mycpu --quiet --load "$ELF_PATH" --trace-json stdout --max-cycles "$MAX_CYCLES"
