#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

RUN_TAG="$(date +%Y%m%d)"
CYCLES=2000000
OUT_DIR=""

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
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [[ -z "$OUT_DIR" ]]; then
  OUT_DIR="docs/cache_matrix/${RUN_TAG}"
fi
mkdir -p "$OUT_DIR"

run_one() {
  local policy="$1"
  shift
  local csv_path="${OUT_DIR}/rv32ui_${policy}.csv"
  echo "[matrix] running policy=${policy} csv=${csv_path}"
  bash test_all.sh -c "$CYCLES" --csv "$csv_path" "$@"
}

# Four cache policy combinations + no-cache baseline.
run_one "wb_wa" --cache-write-back --cache-write-allocate --cache-penalty 10 --quiet
run_one "wb_nowa" --cache-write-back --cache-no-write-allocate --cache-penalty 10 --quiet
run_one "wt_wa" --cache-write-through --cache-write-allocate --cache-penalty 10 --quiet
run_one "wt_nowa" --cache-write-through --cache-no-write-allocate --cache-penalty 10 --quiet
run_one "nocache" --no-cache --cache-penalty 10 --quiet

python3 - "$OUT_DIR" <<'PY'
import csv
import os
import sys
from statistics import mean

out_dir = sys.argv[1]
policies = ["wb_wa", "wb_nowa", "wt_wa", "wt_nowa", "nocache"]

rows_by_policy = {}
for p in policies:
    path = os.path.join(out_dir, f"rv32ui_{p}.csv")
    with open(path, newline="", encoding="utf-8") as f:
        rows_by_policy[p] = list(csv.DictReader(f))

nc_cycles = {r["test"]: int(float(r["cycles"])) for r in rows_by_policy["nocache"]}

summary_path = os.path.join(out_dir, "policy_summary.csv")
detail_path = os.path.join(out_dir, "matrix_detail.csv")

with open(summary_path, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow([
        "policy",
        "tests",
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

    for p in policies:
        rows = rows_by_policy[p]
        tests = len(rows)
        passed = sum(1 for r in rows if int(float(r["rc"])) == 0)
        failed = tests - passed
        avg_cycles = mean(int(float(r["cycles"])) for r in rows) if rows else 0.0
        avg_i = mean(float(r.get("i_cache_hit_pct", 0.0)) for r in rows) if rows else 0.0
        avg_d = mean(float(r.get("d_cache_hit_pct", 0.0)) for r in rows) if rows else 0.0
        avg_stall = mean(int(float(r.get("stall", 0))) for r in rows) if rows else 0.0
        avg_cache_stall = mean(int(float(r.get("cache_stall", 0))) for r in rows) if rows else 0.0
        avg_hazard_stall = mean(int(float(r.get("hazard_stall", 0))) for r in rows) if rows else 0.0
        avg_i_cold = mean(int(float(r.get("i_cold_miss", 0))) for r in rows) if rows else 0.0
        avg_i_conflict = mean(int(float(r.get("i_conflict_miss", 0))) for r in rows) if rows else 0.0
        avg_i_capacity = mean(int(float(r.get("i_capacity_miss", 0))) for r in rows) if rows else 0.0
        avg_d_cold = mean(int(float(r.get("d_cold_miss", 0))) for r in rows) if rows else 0.0
        avg_d_conflict = mean(int(float(r.get("d_conflict_miss", 0))) for r in rows) if rows else 0.0
        avg_d_capacity = mean(int(float(r.get("d_capacity_miss", 0))) for r in rows) if rows else 0.0

        speedups = []
        for r in rows:
            c = int(float(r["cycles"]))
            nc = nc_cycles.get(r["test"], 0)
            if c > 0 and nc > 0:
                speedups.append(nc / c)
        avg_speedup = mean(speedups) if speedups else 0.0

        w.writerow([
            p,
            tests,
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

with open(detail_path, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow([
        "policy",
        "test",
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
    ])

    for p in policies:
        for r in rows_by_policy[p]:
            c = int(float(r["cycles"]))
            nc = nc_cycles.get(r["test"], 0)
            speedup = (nc / c) if c > 0 and nc > 0 else 0.0
            w.writerow([
                p,
                r["test"],
                r["rc"],
                r["cycles"],
                r.get("instrs", 0),
                r.get("i_cache_hit_pct", 0),
                r.get("d_cache_hit_pct", 0),
                r.get("stall", 0),
                r.get("cache_stall", 0),
                r.get("hazard_stall", 0),
                r.get("i_cold_miss", 0),
                r.get("i_conflict_miss", 0),
                r.get("i_capacity_miss", 0),
                r.get("d_cold_miss", 0),
                r.get("d_conflict_miss", 0),
                r.get("d_capacity_miss", 0),
                f"{speedup:.6f}",
            ])

print("WROTE", summary_path)
print("WROTE", detail_path)
PY

echo "[matrix] done: ${OUT_DIR}"
