#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

RUN_TAG="$(date +%Y%m%d)"
CYCLES=2000000
OUT_DIR=""
BASELINE="wb_wa"
SUMMARY_PATH=""
ALLOW_WARN=0
SKIP_BUILD=0
SKIP_CTEST=0
SKIP_MATRIX=0

GATE_ARGS=()

usage() {
  cat <<'EOF'
Usage: tools/run_cache_gate_local.sh [options]

Options:
  --run-tag <tag>                  Run tag used in output folder (default: YYYYMMDD)
  --cycles <n>                     Cycle limit passed to matrix runner (default: 2000000)
  --out-dir <path>                 Matrix output dir (default: docs/cache_matrix/<run-tag>)
  --baseline <policy>              Gate baseline policy (default: wb_wa)
  --summary <path>                 Use existing policy_summary.csv path
  --allow-warn                     Treat WARN as pass (default: WARN blocks)
  --skip-build                     Skip cmake configure/build step
  --skip-ctest                     Skip ctest step
  --skip-matrix                    Skip matrix run, only check gate using summary

Gate threshold overrides (forwarded to check_cache_gate.py):
  --max-cycle-regress-pct <f>
  --max-ihit-drop-pct <f>
  --max-dhit-drop-pct <f>
  --min-cache-stall-ratio <f>

Examples:
  ./tools/run_cache_gate_local.sh --run-tag 20260413
  ./tools/run_cache_gate_local.sh --skip-build --skip-ctest --skip-matrix \
    --summary docs/cache_matrix/20260413/policy_summary.csv
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --run-tag)
      RUN_TAG="$2"
      shift 2
      ;;
    --cycles)
      CYCLES="$2"
      shift 2
      ;;
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    --baseline)
      BASELINE="$2"
      shift 2
      ;;
    --summary)
      SUMMARY_PATH="$2"
      shift 2
      ;;
    --allow-warn)
      ALLOW_WARN=1
      shift
      ;;
    --skip-build)
      SKIP_BUILD=1
      shift
      ;;
    --skip-ctest)
      SKIP_CTEST=1
      shift
      ;;
    --skip-matrix)
      SKIP_MATRIX=1
      shift
      ;;
    --max-cycle-regress-pct|--max-ihit-drop-pct|--max-dhit-drop-pct|--min-cache-stall-ratio)
      GATE_ARGS+=("$1" "$2")
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

if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="docs/cache_matrix/${RUN_TAG}"
fi

if [[ "$SKIP_BUILD" -eq 0 ]]; then
  echo "[gate-local] configuring and building project"
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j
else
  echo "[gate-local] skip build"
fi

if [[ "$SKIP_CTEST" -eq 0 ]]; then
  echo "[gate-local] running ctest"
  ctest --test-dir build --output-on-failure
else
  echo "[gate-local] skip ctest"
fi

if [[ "$SKIP_MATRIX" -eq 0 ]]; then
  echo "[gate-local] running cache policy matrix"
  bash tools/run_cache_matrix.sh --run-tag "$RUN_TAG" --cycles "$CYCLES" --out-dir "$OUT_DIR"
else
  echo "[gate-local] skip matrix"
fi

if [[ -z "$SUMMARY_PATH" ]]; then
  SUMMARY_PATH="${OUT_DIR}/policy_summary.csv"
fi

if [[ ! -f "$SUMMARY_PATH" ]]; then
  echo "[gate-local] summary not found: $SUMMARY_PATH" >&2
  exit 3
fi

if [[ "$SUMMARY_PATH" == *.csv ]]; then
  SUMMARY_DIR="$(cd "$(dirname "$SUMMARY_PATH")" && pwd)"
else
  SUMMARY_DIR="$(cd "$SUMMARY_PATH" && pwd)"
fi

OUTPUT_PREFIX="${SUMMARY_DIR}/gate"

echo "[gate-local] evaluating gate with baseline=$BASELINE"
set +e
python3 tools/check_cache_gate.py \
  --summary "$SUMMARY_PATH" \
  --baseline "$BASELINE" \
  --output-prefix "$OUTPUT_PREFIX" \
  "${GATE_ARGS[@]}"
GATE_RC=$?
set -e

if [[ "$GATE_RC" -eq 1 && "$ALLOW_WARN" -eq 1 ]]; then
  echo "[gate-local] gate WARN tolerated by --allow-warn"
  exit 0
fi

if [[ "$GATE_RC" -ne 0 ]]; then
  echo "[gate-local] gate blocked with rc=$GATE_RC" >&2
  exit "$GATE_RC"
fi

echo "[gate-local] gate PASS"
echo "[gate-local] artifacts: ${SUMMARY_DIR}/policy_summary.csv, ${OUTPUT_PREFIX}_result.json, ${OUTPUT_PREFIX}_report.md"
