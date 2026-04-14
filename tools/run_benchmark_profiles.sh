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
Usage: tools/run_benchmark_profiles.sh [options]

Options:
  --run-tag <tag>       Run tag used in output folder (default: YYYYMMDD)
  --cycles <n>          Cycle limit for each benchmark run (default: 50000000)
  --out-dir <path>      Output dir for benchmark CSV files (default: docs/benchmark/<run-tag>)
  --log-dir <path>      Log dir for raw stdout logs (default: tmp/benchmark_run_<run-tag>)

Outputs:
  benchmark_p1.csv
  benchmark_p10.csv
  benchmark_nocache.csv
  benchmark_detail.csv
  benchmark_summary.csv
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
  OUT_DIR="docs/benchmark/${RUN_TAG}"
fi
if [[ -z "$LOG_DIR" ]]; then
  LOG_DIR="tmp/benchmark_run_${RUN_TAG}"
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

P1_CSV="${OUT_DIR}/benchmark_p1.csv"
P10_CSV="${OUT_DIR}/benchmark_p10.csv"
NC_CSV="${OUT_DIR}/benchmark_nocache.csv"
DETAIL_CSV="${OUT_DIR}/benchmark_detail.csv"
SUMMARY_CSV="${OUT_DIR}/benchmark_summary.csv"

HEADER="benchmark,profile,ms,rc,cycles,instrs,i_cache_hit_pct,d_cache_hit_pct,stall,cache_stall,hazard_stall,i_cold_miss,i_conflict_miss,i_capacity_miss,d_cold_miss,d_conflict_miss,d_capacity_miss,checksum,log_path"
echo "$HEADER" > "$P1_CSV"
echo "$HEADER" > "$P10_CSV"
echo "$HEADER" > "$NC_CSV"

run_profile() {
  local profile="$1"
  local csv_path="$2"
  shift 2
  local -a emu_args=("$@")

  echo "[bench] running profile=${profile}"
  for item in "${BENCHES[@]}"; do
    local bench="${item%%:*}"
    local elf="${item#*:}"

    if [[ ! -f "$elf" ]]; then
      echo "[bench] missing benchmark ELF: $elf" >&2
      echo "${bench},${profile},0,127,0,0,0,0,0,0,0,0,0,0,0,0,0,," >> "$csv_path"
      continue
    fi

    local log_path="${LOG_DIR}/${bench}_${profile}.log"
    echo "[bench]   ${bench} (${profile})"

    local t0 t1 ms rc out
    t0=$(date +%s%N)
    set +e
    out="$($EMULATOR --quiet --load "$elf" -c "$CYCLES" "${emu_args[@]}" 2>&1)"
    rc=$?
    set -e
    t1=$(date +%s%N)
    ms=$(( (t1 - t0)/1000000 ))

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

    echo "${bench},${profile},${ms},${rc},${cycles},${instrs},${i_pct},${d_pct},${stall},${cache_stall},${hazard_stall},${i_cold},${i_conflict},${i_capacity},${d_cold},${d_conflict},${d_capacity},${checksum},${log_path}" >> "$csv_path"
  done
}

run_profile "p1" "$P1_CSV" --cache-penalty 1
run_profile "p10" "$P10_CSV" --cache-penalty 10
run_profile "nocache" "$NC_CSV" --no-cache --cache-penalty 10

python3 - "$P1_CSV" "$P10_CSV" "$NC_CSV" "$DETAIL_CSV" "$SUMMARY_CSV" <<'PY'
import csv
import sys

p1_csv, p10_csv, nc_csv, detail_csv, summary_csv = sys.argv[1:6]


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


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


rows_p1 = read_rows(p1_csv)
rows_p10 = read_rows(p10_csv)
rows_nc = read_rows(nc_csv)

with open(detail_csv, "w", newline="", encoding="utf-8") as f:
    if rows_p1:
        header = rows_p1[0].keys()
    elif rows_p10:
        header = rows_p10[0].keys()
    elif rows_nc:
        header = rows_nc[0].keys()
    else:
        header = []
    w = csv.DictWriter(f, fieldnames=list(header))
    if header:
        w.writeheader()
    for row in rows_p1 + rows_p10 + rows_nc:
        w.writerow(row)

by_p1 = {r["benchmark"]: r for r in rows_p1}
by_p10 = {r["benchmark"]: r for r in rows_p10}
by_nc = {r["benchmark"]: r for r in rows_nc}

benchmarks = sorted(set(by_p1.keys()) | set(by_p10.keys()) | set(by_nc.keys()))

with open(summary_csv, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow([
        "benchmark",
        "rc_p1",
        "rc_p10",
        "rc_nocache",
        "ms_p1",
        "ms_p10",
        "ms_nocache",
        "cycles_p1",
        "cycles_p10",
        "cycles_nocache",
        "instrs_p1",
        "instrs_p10",
        "instrs_nocache",
        "i_hit_p10",
        "d_hit_p10",
        "stall_p10",
        "cache_stall_p10",
        "hazard_stall_p10",
        "i_cold_miss_p10",
        "i_conflict_miss_p10",
        "i_capacity_miss_p10",
        "d_cold_miss_p10",
        "d_conflict_miss_p10",
        "d_capacity_miss_p10",
        "speedup_p10",
        "speedup_p1",
        "penalty_ratio_p10_over_p1",
        "checksum_p10",
    ])

    for b in benchmarks:
        r1 = by_p1.get(b, {})
        r10 = by_p10.get(b, {})
        rn = by_nc.get(b, {})

        c1 = to_int(r1.get("cycles"))
        c10 = to_int(r10.get("cycles"))
        cn = to_int(rn.get("cycles"))

        speedup_p10 = (cn / c10) if c10 > 0 else 0.0
        speedup_p1 = (cn / c1) if c1 > 0 else 0.0
        penalty_ratio = (c10 / c1) if c1 > 0 else 0.0

        w.writerow([
            b,
            to_int(r1.get("rc"), 127),
            to_int(r10.get("rc"), 127),
            to_int(rn.get("rc"), 127),
            to_int(r1.get("ms")),
            to_int(r10.get("ms")),
            to_int(rn.get("ms")),
            c1,
            c10,
            cn,
            to_int(r1.get("instrs")),
            to_int(r10.get("instrs")),
            to_int(rn.get("instrs")),
            to_float(r10.get("i_cache_hit_pct")),
            to_float(r10.get("d_cache_hit_pct")),
            to_int(r10.get("stall")),
            to_int(r10.get("cache_stall")),
            to_int(r10.get("hazard_stall")),
            to_int(r10.get("i_cold_miss")),
            to_int(r10.get("i_conflict_miss")),
            to_int(r10.get("i_capacity_miss")),
            to_int(r10.get("d_cold_miss")),
            to_int(r10.get("d_conflict_miss")),
            to_int(r10.get("d_capacity_miss")),
            f"{speedup_p10:.6f}",
            f"{speedup_p1:.6f}",
            f"{penalty_ratio:.6f}",
            r10.get("checksum", ""),
        ])

print("WROTE", detail_csv)
print("WROTE", summary_csv)
PY

echo "[bench] wrote ${P1_CSV}"
echo "[bench] wrote ${P10_CSV}"
echo "[bench] wrote ${NC_CSV}"
echo "[bench] done: ${OUT_DIR}"
