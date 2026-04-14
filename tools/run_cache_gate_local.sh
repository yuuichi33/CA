#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

RUN_TAG="$(date +%Y%m%d)"
CYCLES=2000000
OUT_DIR=""
BASELINE="wb_wa"
SUMMARY_PATH=""
BENCH_CYCLES=50000000
BENCH_OUT_DIR=""
BENCH_SUMMARY_PATH=""
BENCH_MATRIX_OUT_DIR=""
BENCH_MATRIX_SUMMARY_PATH=""
ALLOW_WARN=0
SKIP_BUILD=0
SKIP_CTEST=0
SKIP_MATRIX=0
SKIP_BENCHMARK=0
SKIP_BENCH_GATE=0
SKIP_BENCH_MATRIX=0
SKIP_BENCH_CACHE_GATE=0

GATE_ARGS=()
BENCH_GATE_ARGS=()
BENCH_CACHE_GATE_ARGS=()

usage() {
  cat <<'EOF'
Usage: tools/run_cache_gate_local.sh [options]

Options:
  --run-tag <tag>                  Run tag used in output folder (default: YYYYMMDD)
  --cycles <n>                     Cycle limit passed to matrix runner (default: 2000000)
  --out-dir <path>                 Matrix output dir (default: docs/cache_matrix/<run-tag>)
  --baseline <policy>              Gate baseline policy (default: wb_wa)
  --summary <path>                 Use existing policy_summary.csv path
  --bench-cycles <n>               Cycle limit for benchmark runs (default: 50000000)
  --bench-out-dir <path>           Benchmark output dir (default: docs/benchmark/<run-tag>)
  --bench-summary <path>           Use existing benchmark_summary.csv path
  --bench-matrix-out-dir <path>    Benchmark cache-matrix dir (default: docs/cache_matrix/<run-tag>)
  --bench-matrix-summary <path>    Use existing benchmark_policy_summary.csv path
  --allow-warn                     Treat WARN as pass (default: WARN blocks)
  --skip-build                     Skip cmake configure/build step
  --skip-ctest                     Skip ctest step
  --skip-matrix                    Skip rv32ui+benchmark matrix run, only check gate using summary
  --skip-benchmark                 Skip benchmark run
  --skip-benchmark-gate            Skip benchmark gate check
  --skip-benchmark-matrix          Skip benchmark cache-matrix run
  --skip-benchmark-cache-gate      Skip benchmark cache-matrix gate check

Gate threshold overrides (forwarded to check_cache_gate.py):
  --max-cycle-regress-pct <f>
  --max-ihit-drop-pct <f>
  --max-dhit-drop-pct <f>
  --min-cache-stall-ratio <f>

Benchmark gate threshold overrides (forwarded to check_benchmark_gate.py):
  --bench-fail-min-speedup-p10 <f>
  --bench-warn-min-speedup-p10 <f>
  --bench-fail-max-penalty-ratio <f>
  --bench-warn-max-penalty-ratio <f>

Benchmark cache-matrix gate overrides (forwarded to check_benchmark_cache_gate.py):
  --bench-cache-fail-cycle-regress-pct <f>
  --bench-cache-warn-cycle-regress-pct <f>
  --bench-cache-fail-speedup-drop-pct <f>
  --bench-cache-warn-speedup-drop-pct <f>
  --bench-cache-fail-dhit-drop-pct <f>
  --bench-cache-warn-dhit-drop-pct <f>

Examples:
  ./tools/run_cache_gate_local.sh --run-tag 20260413
  ./tools/run_cache_gate_local.sh --skip-build --skip-ctest --skip-matrix \
    --summary docs/cache_matrix/20260413/policy_summary.csv
  ./tools/run_cache_gate_local.sh --run-tag 20260414 --bench-cycles 50000000
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
    --bench-cycles)
      BENCH_CYCLES="$2"
      shift 2
      ;;
    --bench-out-dir)
      BENCH_OUT_DIR="$2"
      shift 2
      ;;
    --bench-summary)
      BENCH_SUMMARY_PATH="$2"
      shift 2
      ;;
    --bench-matrix-out-dir)
      BENCH_MATRIX_OUT_DIR="$2"
      shift 2
      ;;
    --bench-matrix-summary)
      BENCH_MATRIX_SUMMARY_PATH="$2"
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
      SKIP_BENCH_MATRIX=1
      shift
      ;;
    --skip-benchmark)
      SKIP_BENCHMARK=1
      shift
      ;;
    --skip-benchmark-gate)
      SKIP_BENCH_GATE=1
      shift
      ;;
    --skip-benchmark-matrix)
      SKIP_BENCH_MATRIX=1
      shift
      ;;
    --skip-benchmark-cache-gate)
      SKIP_BENCH_CACHE_GATE=1
      shift
      ;;
    --max-cycle-regress-pct|--max-ihit-drop-pct|--max-dhit-drop-pct|--min-cache-stall-ratio)
      GATE_ARGS+=("$1" "$2")
      shift 2
      ;;
    --bench-fail-min-speedup-p10|--bench-warn-min-speedup-p10|--bench-fail-max-penalty-ratio|--bench-warn-max-penalty-ratio)
      BENCH_GATE_ARGS+=("--${1#--bench-}" "$2")
      shift 2
      ;;
    --bench-cache-fail-cycle-regress-pct|--bench-cache-warn-cycle-regress-pct|--bench-cache-fail-speedup-drop-pct|--bench-cache-warn-speedup-drop-pct|--bench-cache-fail-dhit-drop-pct|--bench-cache-warn-dhit-drop-pct)
      BENCH_CACHE_GATE_ARGS+=("--${1#--bench-cache-}" "$2")
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

if [[ -z "$BENCH_OUT_DIR" ]]; then
  BENCH_OUT_DIR="docs/benchmark/${RUN_TAG}"
fi

if [[ -z "$BENCH_MATRIX_OUT_DIR" ]]; then
  BENCH_MATRIX_OUT_DIR="$OUT_DIR"
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
  echo "[gate-local] cache gate WARN tolerated by --allow-warn"
elif [[ "$GATE_RC" -ne 0 ]]; then
  echo "[gate-local] cache gate blocked with rc=$GATE_RC" >&2
  exit "$GATE_RC"
fi

if [[ "$SKIP_BENCHMARK" -eq 0 ]]; then
  echo "[gate-local] running benchmark profiles"
  bash tools/run_benchmark_profiles.sh --run-tag "$RUN_TAG" --cycles "$BENCH_CYCLES" --out-dir "$BENCH_OUT_DIR"
else
  echo "[gate-local] skip benchmark"
fi

if [[ -z "$BENCH_SUMMARY_PATH" ]]; then
  BENCH_SUMMARY_PATH="${BENCH_OUT_DIR}/benchmark_summary.csv"
fi

BENCH_SUMMARY_DIR=""
BENCH_OUTPUT_PREFIX=""
BENCH_CACHE_SUMMARY_DIR=""
BENCH_CACHE_OUTPUT_PREFIX=""

if [[ "$SKIP_BENCH_GATE" -eq 0 ]]; then
  if [[ ! -f "$BENCH_SUMMARY_PATH" ]]; then
    echo "[gate-local] benchmark summary not found: $BENCH_SUMMARY_PATH" >&2
    exit 4
  fi

  if [[ "$BENCH_SUMMARY_PATH" == *.csv ]]; then
    BENCH_SUMMARY_DIR="$(cd "$(dirname "$BENCH_SUMMARY_PATH")" && pwd)"
  else
    BENCH_SUMMARY_DIR="$(cd "$BENCH_SUMMARY_PATH" && pwd)"
  fi

  BENCH_OUTPUT_PREFIX="${BENCH_SUMMARY_DIR}/benchmark_gate"

  echo "[gate-local] evaluating benchmark gate"
  set +e
  python3 tools/check_benchmark_gate.py \
    --summary "$BENCH_SUMMARY_PATH" \
    --output-prefix "$BENCH_OUTPUT_PREFIX" \
    "${BENCH_GATE_ARGS[@]}"
  BENCH_GATE_RC=$?
  set -e

  if [[ "$BENCH_GATE_RC" -eq 1 && "$ALLOW_WARN" -eq 1 ]]; then
    echo "[gate-local] benchmark gate WARN tolerated by --allow-warn"
  elif [[ "$BENCH_GATE_RC" -ne 0 ]]; then
    echo "[gate-local] benchmark gate blocked with rc=$BENCH_GATE_RC" >&2
    exit "$BENCH_GATE_RC"
  fi
else
  echo "[gate-local] skip benchmark gate"
fi

if [[ "$SKIP_BENCH_MATRIX" -eq 0 ]]; then
  echo "[gate-local] running benchmark cache policy matrix"
  bash tools/run_benchmark_cache_matrix.sh --run-tag "$RUN_TAG" --cycles "$BENCH_CYCLES" --out-dir "$BENCH_MATRIX_OUT_DIR"
else
  echo "[gate-local] skip benchmark cache matrix"
fi

if [[ -z "$BENCH_MATRIX_SUMMARY_PATH" ]]; then
  BENCH_MATRIX_SUMMARY_PATH="${BENCH_MATRIX_OUT_DIR}/benchmark_policy_summary.csv"
fi

if [[ "$SKIP_BENCH_CACHE_GATE" -eq 0 ]]; then
  if [[ ! -f "$BENCH_MATRIX_SUMMARY_PATH" ]]; then
    echo "[gate-local] benchmark matrix summary not found: $BENCH_MATRIX_SUMMARY_PATH" >&2
    exit 5
  fi

  if [[ "$BENCH_MATRIX_SUMMARY_PATH" == *.csv ]]; then
    BENCH_CACHE_SUMMARY_DIR="$(cd "$(dirname "$BENCH_MATRIX_SUMMARY_PATH")" && pwd)"
  else
    BENCH_CACHE_SUMMARY_DIR="$(cd "$BENCH_MATRIX_SUMMARY_PATH" && pwd)"
  fi

  BENCH_CACHE_OUTPUT_PREFIX="${BENCH_CACHE_SUMMARY_DIR}/benchmark_cache_gate"

  echo "[gate-local] evaluating benchmark cache matrix gate"
  set +e
  python3 tools/check_benchmark_cache_gate.py \
    --summary "$BENCH_MATRIX_SUMMARY_PATH" \
    --baseline "$BASELINE" \
    --output-prefix "$BENCH_CACHE_OUTPUT_PREFIX" \
    "${BENCH_CACHE_GATE_ARGS[@]}"
  BENCH_CACHE_GATE_RC=$?
  set -e

  if [[ "$BENCH_CACHE_GATE_RC" -eq 1 && "$ALLOW_WARN" -eq 1 ]]; then
    echo "[gate-local] benchmark cache gate WARN tolerated by --allow-warn"
  elif [[ "$BENCH_CACHE_GATE_RC" -ne 0 ]]; then
    echo "[gate-local] benchmark cache gate blocked with rc=$BENCH_CACHE_GATE_RC" >&2
    exit "$BENCH_CACHE_GATE_RC"
  fi
else
  echo "[gate-local] skip benchmark cache gate"
fi

echo "[gate-local] cache gate PASS"
echo "[gate-local] artifacts(cache): ${SUMMARY_DIR}/policy_summary.csv, ${OUTPUT_PREFIX}_result.json, ${OUTPUT_PREFIX}_report.md"
if [[ -n "$BENCH_SUMMARY_DIR" ]]; then
  echo "[gate-local] artifacts(benchmark): ${BENCH_SUMMARY_DIR}/benchmark_summary.csv, ${BENCH_OUTPUT_PREFIX}_result.json, ${BENCH_OUTPUT_PREFIX}_report.md"
fi
if [[ -n "$BENCH_CACHE_SUMMARY_DIR" ]]; then
  echo "[gate-local] artifacts(benchmark-cache-matrix): ${BENCH_CACHE_SUMMARY_DIR}/benchmark_policy_summary.csv, ${BENCH_CACHE_OUTPUT_PREFIX}_result.json, ${BENCH_CACHE_OUTPUT_PREFIX}_report.md"
fi
