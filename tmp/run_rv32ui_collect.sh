#!/usr/bin/env bash
set -euo pipefail

mkdir -p docs
echo "test,ms,rc,cycles,instrs,i_cache_hit_pct,d_cache_hit_pct" > docs/rv32ui_perf.csv

for f in riscv-tests/isa/rv32ui-p-*; do
  [ -f "$f" ] || continue
  name=$(basename "$f")
  printf "Running %s\n" "$name"
  t0=$(date +%s%N)
  OUT=$(./build/mycpu -q -l "$f" -c 2000000 2>&1) ; rc=$?
  t1=$(date +%s%N)
  ms=$(( (t1 - t0)/1000000 ))
  cycles=$(echo "$OUT" | sed -n 's/.*Cycles: *\([0-9]*\).*/\1/p')
  instrs=$(echo "$OUT" | sed -n 's/.*Instrs: *\([0-9]*\).*/\1/p')
  i_pct=$(echo "$OUT" | sed -n 's/.*I-Cache Hit: *\([0-9.]*\)%.*/\1/p')
  d_pct=$(echo "$OUT" | sed -n 's/.*D-Cache Hit: *\([0-9.]*\)%.*/\1/p')
  cycles=${cycles:-0}
  instrs=${instrs:-0}
  i_pct=${i_pct:-0}
  d_pct=${d_pct:-0}
  echo "$name,$ms,$rc,$cycles,$instrs,$i_pct,$d_pct" >> docs/rv32ui_perf.csv
done

awk -F, 'NR>1{sum+=$2; if(min==0||$2<min)min=$2; if($2>max)max=$2; count++; cycles+=$4; instrs+=$5; i_sum+=$6; d_sum+=$7} END{if(count>0) printf("COUNT=%d TOTAL_MS=%d MIN_MS=%d MAX_MS=%d AVG_MS=%.2f TOTAL_CYCLES=%d TOTAL_INSTRS=%d AVG_IPC=%.5f AVG_I_HIT=%.2f AVG_D_HIT=%.2f\n",count,sum,min,max,sum/count,cycles,instrs,(cycles>0?instrs/cycles:0),i_sum/count,d_sum/count); else print "no data"}' docs/rv32ui_perf.csv
