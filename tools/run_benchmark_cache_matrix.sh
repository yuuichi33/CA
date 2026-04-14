#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

EMULATOR="./build/mycpu"
RUN_TAG="$(date +%Y%m%d)"
CYCLES=50000000
OUT_DIR=""
LOG_DIR=""

usage() {
  cat <<'EOF'
Usage: tools/run_benchmark_cache_matrix.sh [options]

Options:
  --run-tag <tag>       Run tag used in output folder (default: YYYYMMDD)
  --cycles <n>          Cycle limit for each benchmark run (default: 50000000)
  --out-dir <path>      Output dir for matrix CSV files (default: docs/cache_matrix/<run-tag>)
  --log-dir <path>      Log dir for raw stdout logs (default: tmp/benchmark_matrix_<run-tag>)

Outputs:
  benchmark_wb_wa.csv
  benchmark_wb_nowa.csv
  benchmark_wt_wa.csv
  benchmark_wt_nowa.csv
  benchmark_nocache.csv
  benchmark_matrix_detail.csv
  benchmark_policy_summary.csv
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
    --log-dir)
      LOG_DIR="$2"
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
if [[ -z "$LOG_DIR" ]]; then
  LOG_DIR="tmp/benchmark_matrix_${RUN_TAG}"
fi

mkdir -p "$OUT_DIR" "$LOG_DIR"

if [[ ! -x "$EMULATOR" ]]; then
  echo "Error: emulator $EMULATOR not found or not executable. Build first." >&2
  exit 2
fi

BENCHES=(
  "hello:benchmarks/hello.elf"
  "matmul:benchmarks/matmul.elf"
  "quicksort_stress:benchmarks/quicksort_stress.elf"
)

POLICIES=(
  "wb_wa"
  "wb_nowa"
  "wt_wa"
  "wt_nowa"
  "nocache"
)

HEADER="benchmark,policy,ms,rc,cycles,instrs,i_cache_hit_pct,d_cache_hit_pct,stall,cache_stall,hazard_stall,i_cold_miss,i_conflict_miss,i_capacity_miss,d_cold_miss,d_conflict_miss,d_capacity_miss,checksum,log_path"

for policy in "${POLICIES[@]}"; do
  echo "$HEADER" > "${OUT_DIR}/benchmark_${policy}.csv"
done

run_policy() {
  local policy="$1"
  shift
  local csv_path="${OUT_DIR}/benchmark_${policy}.csv"
  local -a emu_args=("$@")

  echo "[bench-matrix] running policy=${policy}"

  for item in "${BENCHES[@]}"; do
    local bench="${item%%:*}"
    local elf="${item#*:}"

    if [[ ! -f "$elf" ]]; then
      echo "[bench-matrix] missing benchmark ELF: $elf" >&2
      echo "${bench},${policy},0,127,0,0,0,0,0,0,0,0,0,0,0,0,0,," >> "$csv_path"
      continue
    fi

    local log_path="${LOG_DIR}/${bench}_${policy}.log"
    echo "[bench-matrix]   ${bench} (${policy})"

    local t0 t1 ms rc out
    t0=$(date +%s%N)
    set +e
    out="$($EMULATOR --quiet --load "$elf" -c "$CYCLES" "${emu_args[@]}" 2>&1)"
    rc=$?
    set -e
    t1=$(date +%s%N)
    ms=$(((t1 - t0) / 1000000))

    printf "%s\n" "$out" > "$log_path"

    local cycles instrs i_pct d_pct stall cache_stall hazard_stall
    local i_cold i_conflict i_capacity d_cold d_conflict d_capacity checksum

    cycles=$(echo "$out" | sed -n 's/.*Cycles: *\([0-9]*\).*/\1/p' | tail -n 1)
    instrs=$(echo "$out" | sed -n 's/.*Instrs: *\([0-9]*\).*/\1/p' | tail -n 1)
    i_pct=$(echo "$out" | sed -n 's/.*I-Cache Hit: *\([0-9.]*\)%.*/\1/p' | tail -n 1)
    d_pct=$(echo "$out" | sed -n 's/.*D-Cache Hit: *\([0-9.]*\)%.*/\1/p' | tail -n 1)
    stall=$(echo "$out" | sed -n 's/.*Stall: *\([0-9]*\).*/\1/p' | tail -n 1)
    cache_stall=$(echo "$out" | sed -n 's/.*CacheStall: *\([0-9]*\).*/\1/p' | tail -n 1)
    hazard_stall=$(echo "$out" | sed -n 's/.*HazardStall: *\([0-9]*\).*/\1/p' | tail -n 1)
    i_cold=$(echo "$out" | sed -n 's/.*I-ColdMiss: *\([0-9]*\).*/\1/p' | tail -n 1)
    i_conflict=$(echo "$out" | sed -n 's/.*I-ConflictMiss: *\([0-9]*\).*/\1/p' | tail -n 1)
    i_capacity=$(echo "$out" | sed -n 's/.*I-CapacityMiss: *\([0-9]*\).*/\1/p' | tail -n 1)
    d_cold=$(echo "$out" | sed -n 's/.*D-ColdMiss: *\([0-9]*\).*/\1/p' | tail -n 1)
    d_conflict=$(echo "$out" | sed -n 's/.*D-ConflictMiss: *\([0-9]*\).*/\1/p' | tail -n 1)
    d_capacity=$(echo "$out" | sed -n 's/.*D-CapacityMiss: *\([0-9]*\).*/\1/p' | tail -n 1)
    checksum=$(echo "$out" | sed -n 's/.*checksum=0x\([0-9a-fA-F]*\).*/\1/p' | tail -n 1)

    cycles=${cycles:-0}
    instrs=${instrs:-0}
    i_pct=${i_pct:-0}
    d_pct=${d_pct:-0}
    stall=${stall:-0}
    cache_stall=${cache_stall:-0}
    hazard_stall=${hazard_stall:-0}
    i_cold=${i_cold:-0}
    i_conflict=${i_conflict:-0}
    i_capacity=${i_capacity:-0}
    d_cold=${d_cold:-0}
    d_conflict=${d_conflict:-0}
    d_capacity=${d_capacity:-0}
    checksum=${checksum:-}

    echo "${bench},${policy},${ms},${rc},${cycles},${instrs},${i_pct},${d_pct},${stall},${cache_stall},${hazard_stall},${i_cold},${i_conflict},${i_capacity},${d_cold},${d_conflict},${d_capacity},${checksum},${log_path}" >> "$csv_path"
  done
}

run_policy "wb_wa" --cache-write-back --cache-write-allocate --cache-penalty 10
run_policy "wb_nowa" --cache-write-back --cache-no-write-allocate --cache-penalty 10
run_policy "wt_wa" --cache-write-through --cache-write-allocate --cache-penalty 10
run_policy "wt_nowa" --cache-write-through --cache-no-write-allocate --cache-penalty 10
run_policy "nocache" --no-cache --cache-penalty 10

python3 - "$OUT_DIR" <<'PY'
import csv
import os
import sys
from statistics import mean

out_dir = sys.argv[1]
policies = ["wb_wa", "wb_nowa", "wt_wa", "wt_nowa", "nocache"]


def to_int(v, d=0):
    try:
        return int(float(v))
    except Exception:
        return d


def to_float(v, d=0.0):
    try:
        return float(v)
    except Exception:
        return d


rows_by_policy = {}
for policy in policies:
    path = os.path.join(out_dir, f"benchmark_{policy}.csv")
    with open(path, newline="", encoding="utf-8") as f:
        rows_by_policy[policy] = list(csv.DictReader(f))

nc_cycles = {r["benchmark"]: to_int(r.get("cycles")) for r in rows_by_policy["nocache"]}

summary_path = os.path.join(out_dir, "benchmark_policy_summary.csv")
with open(summary_path, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow([
        "policy",
        "benchmarks",
        "pass",
        "fail",
        "avg_cycles",
        "avg_i_hit_pct",
        "avg_d_hit_pct",
        "avg_stall",
        "avg_cache_stall",
        "avg_hazard_stall",
        "avg_i_cold_miss",
        "avg_i_conflict_miss",
        "avg_i_capacity_miss",
        "avg_d_cold_miss",
        "avg_d_conflict_miss",
        "avg_d_capacity_miss",
        "avg_speedup_vs_nocache",
    ])

    for policy in policies:
        rows = rows_by_policy[policy]
        benchmarks = len(rows)
        passed = sum(1 for r in rows if to_int(r.get("rc"), 127) == 0)
        failed = benchmarks - passed
        avg_cycles = mean(to_int(r.get("cycles")) for r in rows) if rows else 0.0
        avg_i = mean(to_float(r.get("i_cache_hit_pct")) for r in rows) if rows else 0.0
        avg_d = mean(to_float(r.get("d_cache_hit_pct")) for r in rows) if rows else 0.0
        avg_stall = mean(to_int(r.get("stall")) for r in rows) if rows else 0.0
        avg_cache_stall = mean(to_int(r.get("cache_stall")) for r in rows) if rows else 0.0
        avg_hazard_stall = mean(to_int(r.get("hazard_stall")) for r in rows) if rows else 0.0
        avg_i_cold = mean(to_int(r.get("i_cold_miss")) for r in rows) if rows else 0.0
        avg_i_conflict = mean(to_int(r.get("i_conflict_miss")) for r in rows) if rows else 0.0
        avg_i_capacity = mean(to_int(r.get("i_capacity_miss")) for r in rows) if rows else 0.0
        avg_d_cold = mean(to_int(r.get("d_cold_miss")) for r in rows) if rows else 0.0
        avg_d_conflict = mean(to_int(r.get("d_conflict_miss")) for r in rows) if rows else 0.0
        avg_d_capacity = mean(to_int(r.get("d_capacity_miss")) for r in rows) if rows else 0.0

        speedups = []
        for r in rows:
            benchmark = r.get("benchmark", "")
            c = to_int(r.get("cycles"))
            nc = nc_cycles.get(benchmark, 0)
            if c > 0 and nc > 0:
                speedups.append(nc / c)
        avg_speedup = mean(speedups) if speedups else 0.0

        w.writerow([
            policy,
            benchmarks,
            passed,
            failed,
            f"{avg_cycles:.2f}",
            f"{avg_i:.2f}",
            f"{avg_d:.2f}",
            f"{avg_stall:.2f}",
            f"{avg_cache_stall:.2f}",
            f"{avg_hazard_stall:.2f}",
            f"{avg_i_cold:.2f}",
            f"{avg_i_conflict:.2f}",
            f"{avg_i_capacity:.2f}",
            f"{avg_d_cold:.2f}",
            f"{avg_d_conflict:.2f}",
            f"{avg_d_capacity:.2f}",
            f"{avg_speedup:.4f}",
        ])

detail_path = os.path.join(out_dir, "benchmark_matrix_detail.csv")
with open(detail_path, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow([
        "policy",
        "benchmark",
        "rc",
        "cycles",
        "instrs",
        "i_cache_hit_pct",
        "d_cache_hit_pct",
        "stall",
        "cache_stall",
        "hazard_stall",
        "i_cold_miss",
        "i_conflict_miss",
        "i_capacity_miss",
        "d_cold_miss",
        "d_conflict_miss",
        "d_capacity_miss",
        "speedup_vs_nocache",
        "checksum",
        "log_path",
    ])

    for policy in policies:
        for row in rows_by_policy[policy]:
            benchmark = row.get("benchmark", "")
            c = to_int(row.get("cycles"))
            nc = nc_cycles.get(benchmark, 0)
            speedup = (nc / c) if c > 0 and nc > 0 else 0.0
            w.writerow([
                policy,
                benchmark,
                row.get("rc", "127"),
                row.get("cycles", "0"),
                row.get("instrs", "0"),
                row.get("i_cache_hit_pct", "0"),
                row.get("d_cache_hit_pct", "0"),
                row.get("stall", "0"),
                row.get("cache_stall", "0"),
                row.get("hazard_stall", "0"),
                row.get("i_cold_miss", "0"),
                row.get("i_conflict_miss", "0"),
                row.get("i_capacity_miss", "0"),
                row.get("d_cold_miss", "0"),
                row.get("d_conflict_miss", "0"),
                row.get("d_capacity_miss", "0"),
                f"{speedup:.6f}",
                row.get("checksum", ""),
                row.get("log_path", ""),
            ])

print("WROTE", summary_path)
print("WROTE", detail_path)
PY

echo "[bench-matrix] done: ${OUT_DIR}"
