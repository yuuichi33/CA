#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

# Simple runner for riscv-tests rv32ui binaries using local mycpu emulator
# Scans riscv-tests/isa/rv32ui-p-* and runs each executable with a cycle limit.

EMULATOR="./build/mycpu"
PATTERN="riscv-tests/isa/rv32ui-p-*"
CYCLES=2000000
CSV_OUT=""
EMU_ARGS=()

while [ $# -gt 0 ]; do
  case "$1" in
    -c|--cycles)
      if [ $# -lt 2 ]; then
        echo "Error: $1 requires a value" >&2
        exit 2
      fi
      CYCLES="$2"
      shift 2
      ;;
    --csv)
      if [ $# -lt 2 ]; then
        echo "Error: --csv requires a file path" >&2
        exit 2
      fi
      CSV_OUT="$2"
      shift 2
      ;;
    --)
      shift
      while [ $# -gt 0 ]; do
        EMU_ARGS+=("$1")
        shift
      done
      ;;
    *)
      EMU_ARGS+=("$1")
      shift
      ;;
  esac
done

if [ ! -x "$EMULATOR" ]; then
  echo "Error: emulator $EMULATOR not found or not executable. Build first:" >&2
  echo "  mkdir -p build && cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build -j" >&2
  exit 2
fi

files=( $PATTERN )
if [ ${#files[@]} -eq 0 ]; then
  echo "No tests found matching pattern: $PATTERN" >&2
  exit 2
fi

if [ -n "$CSV_OUT" ]; then
  mkdir -p "$(dirname "$CSV_OUT")"
  echo "test,ms,rc,cycles,instrs,i_cache_hit_pct,d_cache_hit_pct" > "$CSV_OUT"
fi

total=0
pass=0
fail=0
failed=()

printf "Running %d tests from %s\n\n" "${#files[@]}" "$PATTERN"
for f in "${files[@]}"; do
  # ensure it's executable
  if [ ! -x "$f" ]; then
    printf "[SKIP] %-40s (not executable)\n" "$(basename "$f")"
    continue
  fi

  name=$(basename "$f")
  printf "Running %-40s ... " "$name"

  t0=$(date +%s%N)
  set +e
  OUT="$($EMULATOR -l "$f" -c "$CYCLES" "${EMU_ARGS[@]}" 2>&1)"
  rc=$?
  set -e
  t1=$(date +%s%N)
  ms=$(( (t1 - t0)/1000000 ))

  cycles=$(echo "$OUT" | sed -n 's/.*Cycles: *\([0-9]*\).*/\1/p' | tail -n 1)
  instrs=$(echo "$OUT" | sed -n 's/.*Instrs: *\([0-9]*\).*/\1/p' | tail -n 1)
  i_pct=$(echo "$OUT" | sed -n 's/.*I-Cache Hit: *\([0-9.]*\)%.*/\1/p' | tail -n 1)
  d_pct=$(echo "$OUT" | sed -n 's/.*D-Cache Hit: *\([0-9.]*\)%.*/\1/p' | tail -n 1)
  cycles=${cycles:-0}
  instrs=${instrs:-0}
  i_pct=${i_pct:-0}
  d_pct=${d_pct:-0}

  if [ -n "$CSV_OUT" ]; then
    echo "$name,$ms,$rc,$cycles,$instrs,$i_pct,$d_pct" >> "$CSV_OUT"
  fi

  # rv32ui exits with rc=0 for pass and rc!=0 for fail.
  if [ "$rc" -eq 0 ]; then
    printf "[PASS] %s\n" "$name"
    pass=$((pass+1))
  else
    printf "[FAIL rc=%d] %s\n" "$rc" "$name"
    fail=$((fail+1))
    failed+=("$name")
  fi
  total=$((total+1))
done

echo
echo "================ Test Summary ================"
printf "Total:    %d\n" "$total"
printf "Passed:   %d\n" "$pass"
printf "Failed:   %d\n" "$fail"
if [ "$total" -gt 0 ]; then
  perc=$(( 100 * pass / total ))
  printf "Pass rate: %d%%\n" "$perc"
fi

if [ "$fail" -gt 0 ]; then
  echo
  echo "Failed tests:";
  for t in "${failed[@]}"; do
    echo "  - $t"
  done
  exit 1
fi

exit 0
