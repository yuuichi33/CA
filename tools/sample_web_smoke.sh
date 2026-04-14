#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

RUN_TAG="$(date +%Y%m%d)"
RUN_DIR=""
HOST="127.0.0.1"
PORT="${TRACE_PORT:-8080}"
WEB_ROOT="web"
TIMEOUT=5
MAX_PORT=9000

usage() {
  cat <<'EOF'
Usage: tools/sample_web_smoke.sh [options]

Options:
  --run-tag <tag>       Run tag used in output folder (default: YYYYMMDD)
  --run-dir <path>      Output dir for smoke artifacts (default: tmp/full_run_<run-tag>)
  --host <addr>         Trace server bind host (default: 127.0.0.1)
  --port <n>            Preferred trace server port (default: TRACE_PORT or 8080)
  --web-root <path>     Web root directory (default: web)
  --timeout <sec>       Curl timeout in seconds (default: 5)

Outputs:
  web_health.json
  web_index_head.txt
  web_smoke_status.json
  trace_server.log
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --run-tag)
      RUN_TAG="$2"
      shift 2
      ;;
    --run-dir)
      RUN_DIR="$2"
      shift 2
      ;;
    --host)
      HOST="$2"
      shift 2
      ;;
    --port)
      PORT="$2"
      shift 2
      ;;
    --web-root)
      WEB_ROOT="$2"
      shift 2
      ;;
    --timeout)
      TIMEOUT="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ -z "$RUN_DIR" ]]; then
  RUN_DIR="tmp/full_run_${RUN_TAG}"
fi
mkdir -p "$RUN_DIR"

LOG_PATH="${RUN_DIR}/trace_server.log"
HEALTH_PATH="${RUN_DIR}/web_health.json"
INDEX_PATH="${RUN_DIR}/web_index_head.txt"
STATUS_PATH="${RUN_DIR}/web_smoke_status.json"

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

json_escape() {
  python3 - "$1" <<'PY'
import json
import sys
print(json.dumps(sys.argv[1], ensure_ascii=True))
PY
}

mk_failed_status() {
  local reason="$1"
  local reason_json
  reason_json=$(json_escape "$reason")
  cat > "$STATUS_PATH" <<EOF
{"status":"WARN","reason":${reason_json},"host":"${HOST}","port":${PORT}}
EOF
  cat > "$HEALTH_PATH" <<EOF
{"status":"WARN","reason":${reason_json},"host":"${HOST}","port":${PORT}}
EOF
  if [[ ! -s "$INDEX_PATH" ]]; then
    echo "<!-- WEB SMOKE SAMPLE FAILED -->" > "$INDEX_PATH"
  fi
}

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
  fi
}

trap cleanup EXIT

pkill -f "python3 tools/trace_server.py" >/dev/null 2>&1 || true

while ! is_port_free "$PORT"; do
  PORT=$((PORT + 1))
  if [[ "$PORT" -gt "$MAX_PORT" ]]; then
    echo "[web-smoke] no free port from requested start to ${MAX_PORT}" >&2
    echo "<!-- NO FREE PORT -->" > "$INDEX_PATH"
    mk_failed_status "no free port in scan range"
    exit 0
  fi
done

python3 tools/trace_server.py \
  --host "$HOST" \
  --port "$PORT" \
  --web-root "$WEB_ROOT" \
  1>/dev/null 2>"$LOG_PATH" &
SERVER_PID=$!

READY=0
for _ in $(seq 1 30); do
  if curl -s --max-time 1 "http://${HOST}:${PORT}/health" >/dev/null 2>&1; then
    READY=1
    break
  fi
done

if [[ "$READY" -ne 1 ]]; then
  echo "[web-smoke] trace server not reachable at ${HOST}:${PORT}" >&2
  echo "<!-- TRACE SERVER UNREACHABLE -->" > "$INDEX_PATH"
  mk_failed_status "trace server not reachable after startup"
  exit 0
fi

HEALTH_RAW="$(curl -s --max-time "$TIMEOUT" "http://${HOST}:${PORT}/health" || true)"
if [[ -z "$HEALTH_RAW" ]]; then
  echo "[web-smoke] empty /health response" >&2
  echo "<!-- EMPTY HEALTH RESPONSE -->" > "$INDEX_PATH"
  mk_failed_status "empty /health response"
  exit 0
fi

if ! python3 - <<'PY' "$HEALTH_RAW"
import json
import sys
json.loads(sys.argv[1])
PY
then
  echo "[web-smoke] /health is not valid JSON" >&2
  echo "<!-- INVALID HEALTH JSON -->" > "$INDEX_PATH"
  mk_failed_status "invalid /health json response"
  exit 0
fi

printf "%s\n" "$HEALTH_RAW" > "$HEALTH_PATH"

INDEX_HEAD="$(curl -s --max-time "$TIMEOUT" "http://${HOST}:${PORT}/" | head -n 1 || true)"
if [[ -z "$INDEX_HEAD" ]]; then
  INDEX_HEAD="<!-- EMPTY INDEX RESPONSE -->"
fi
printf "%s\n" "$INDEX_HEAD" > "$INDEX_PATH"

cat > "$STATUS_PATH" <<EOF
{"status":"PASS","reason":"sampled","host":"${HOST}","port":${PORT}}
EOF

echo "[web-smoke] sampled host=${HOST} port=${PORT}"
echo "[web-smoke] wrote ${HEALTH_PATH}"
echo "[web-smoke] wrote ${INDEX_PATH}"
echo "[web-smoke] wrote ${STATUS_PATH}"
