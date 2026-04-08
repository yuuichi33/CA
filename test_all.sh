#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

# Simple runner for riscv-tests rv32ui binaries using local mycpu emulator
# Scans riscv-tests/isa/rv32ui-p-* and runs each executable with a cycle limit.

EMULATOR="./build/mycpu"
PATTERN="riscv-tests/isa/rv32ui-p-*"
CYCLES=2000000

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

  OUT="$($EMULATOR -l "$f" -c "$CYCLES" 2>&1 || true)"
  # detect semihosting signal
  if echo "$OUT" | grep -q "TOHOST:"; then
    printf "[PASS] %s\n" "$name"
    pass=$((pass+1))
  else
    printf "[FAIL] %s\n" "$name"
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
