# myCPU

myCPU is a C++17 RISC-V simulator for teaching and micro-architecture experiments.
It implements an RV32I 5-stage pipeline with privilege support, Sv32 MMU,
I/D cache, MMIO peripherals, ELF loading, and trace visualization.

## Features

- Pipeline: IF / ID / EX / MEM / WB
- ISA: RV32I + CSR/system instructions (ECALL/MRET/SRET/SFENCE.VMA/FENCE/FENCE.I)
- Privilege: M/S/U modes with trap and delegation paths
- MMU: Sv32 page walk + ASID-aware TLB
- Cache: split I-cache/D-cache, configurable write/allocate policies
- Stats: cycles/instrs, I/D hit rate, stall/cache_stall/hazard_stall
- Trace: JSONL + SSE middleware + web dashboard

## Instruction Coverage

- Effective instruction count: 49 (excluding placeholder labels UNKNOWN/SYSTEM)

| Category | Count | Instructions |
| --- | ---: | --- |
| R-type arithmetic | 10 | ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND |
| I-type arithmetic | 9 | ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI |
| Load | 5 | LB, LH, LW, LBU, LHU |
| Store | 3 | SB, SH, SW |
| Branch | 6 | BEQ, BNE, BLT, BGE, BLTU, BGEU |
| U-type | 2 | LUI, AUIPC |
| JAL | 1 | JAL |
| JALR | 1 | JALR |
| CSR/privileged | 10 | ECALL, MRET, SRET, SFENCE.VMA, CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI |
| Fence | 2 | FENCE, FENCE.I |

- Baseline ISA scope: RV32I + CSR/system instructions used by the current privilege/MMU flow.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Full Test Matrix

### Data semantics

- `p1`: cache enabled, `--cache-penalty 1`
- `p10`: cache enabled, `--cache-penalty 10`
- `no-cache`: cache disabled with `--no-cache --cache-penalty 10`
- cache 策略矩阵：`wb_wa` / `wb_nowa` / `wt_wa` / `wt_nowa` / `nocache`

### Run steps

```bash
# 1) ctest
ctest --test-dir build --output-on-failure

# 2) rv32ui (42 tests x 3 profiles)
bash test_all.sh --csv docs/rv32ui_perf_full_p1.csv --cache-penalty 1 --quiet
bash test_all.sh --csv docs/rv32ui_perf_full_p10.csv --cache-penalty 10 --quiet
bash test_all.sh --csv docs/rv32ui_perf_full_nocache.csv --no-cache --cache-penalty 10 --quiet

# 3) benchmark structured data + benchmark gate
bash tools/run_benchmark_profiles.sh --run-tag <run-tag> --cycles 50000000
python3 tools/check_benchmark_gate.py \
  --summary docs/benchmark/<run-tag>/benchmark_summary.csv \
  --output-prefix docs/benchmark/<run-tag>/benchmark_gate

# 3b) benchmark cache-matrix + benchmark cache-gate
bash tools/run_benchmark_cache_matrix.sh --run-tag <run-tag> --cycles 50000000 \
  --out-dir docs/cache_matrix/<run-tag>
python3 tools/check_benchmark_cache_gate.py \
  --summary docs/cache_matrix/<run-tag>/benchmark_policy_summary.csv \
  --baseline wb_wa \
  --output-prefix docs/cache_matrix/<run-tag>/benchmark_cache_gate

# 3c) web smoke sampling (trace_server)
bash tools/sample_web_smoke.sh --run-tag <run-tag>

# 4) generate report + charts
python3 tools/gen_full_test_report.py \
  --run-tag <run-tag> \
  --run-dir tmp/full_run_<run-tag> \
  --cache-matrix-dir docs/cache_matrix/<run-tag> \
  --gate-prefix docs/cache_matrix/<run-tag>/gate \
  --benchmark-dir docs/benchmark/<run-tag> \
  --benchmark-gate-prefix docs/benchmark/<run-tag>/benchmark_gate
```

### Outputs

- `docs/FULL_TEST_PERFORMANCE_REPORT.md`
- `docs/full_test_summary_<run-tag>.csv`
- `docs/figures/full_run_<run-tag>_speedup_bar.png`
- `docs/figures/full_run_<run-tag>_hitrate_scatter.png`
- `docs/figures/full_run_<run-tag>_benchmark_cycles_log.png`
- `docs/cache_matrix/<run-tag>/benchmark_policy_summary.csv`
- `docs/cache_matrix/<run-tag>/benchmark_matrix_detail.csv`
- `docs/cache_matrix/<run-tag>/benchmark_cache_gate_result.json`
- `docs/benchmark/<run-tag>/benchmark_summary.csv`
- `docs/benchmark/<run-tag>/benchmark_gate_result.json`
- `tmp/full_run_<run-tag>/web_health.json`
- `tmp/full_run_<run-tag>/web_index_head.txt`

## Latest Test Results (2026-04-14)

### Correctness

| Scope | Result | Evidence |
| --- | --- | --- |
| CTest | 20/20 PASS | `tmp/full_run_20260413/ctest_full.log` |
| rv32ui p1 | 42/42 PASS | `docs/rv32ui_perf_full_p1.csv` |
| rv32ui p10 | 42/42 PASS | `docs/rv32ui_perf_full_p10.csv` |
| rv32ui no-cache | 42/42 PASS | `docs/rv32ui_perf_full_nocache.csv` |
| benchmark gate | PASS (issues=0) | `docs/benchmark/20260414/benchmark_gate_result.json` |
| benchmark cache gate | PASS (issues=0) | `docs/cache_matrix/20260413/benchmark_cache_gate_result.json` |
| web smoke | PASS | `tmp/full_run_20260413/web_smoke_status.json` |

### Benchmark Snapshot

| Case | Cache cycles | No-cache cycles | Speedup |
| --- | ---: | ---: | ---: |
| hello | 161 | 1518 | 9.43x |
| matmul | 277966 | 3151861 | 11.34x |
| quicksort_stress | 1972901 | 25074548 | 12.71x |

- Summary source: `docs/benchmark/20260414/benchmark_summary.csv`
- Gate report: `docs/benchmark/20260414/benchmark_gate_report.md`

### rv32ui Cache Matrix + Gate

| Policy | pass/tests | avg_cycles | avg_i_hit_pct | avg_d_hit_pct | avg_speedup_vs_nocache |
| --- | ---: | ---: | ---: | ---: | ---: |
| wb_wa | 42/42 | 696.88 | 93.50 | 19.41 | 6.4705 |
| wb_nowa | 42/42 | 698.88 | 93.50 | 18.34 | 6.4553 |
| wt_wa | 42/42 | 698.88 | 93.50 | 18.34 | 6.4553 |
| wt_nowa | 42/42 | 698.88 | 93.50 | 18.34 | 6.4553 |
| nocache | 42/42 | 4633.83 | 0.00 | 0.00 | 1.0000 |

- Gate status: PASS (baseline `wb_wa`, issues 0)
- Artifacts: `docs/cache_matrix/20260413/policy_summary.csv`, `docs/cache_matrix/20260413/gate_result.json`, `docs/cache_matrix/20260413/gate_report.md`

### Benchmark Cache Matrix + Gate

| Policy | pass/benchmarks | avg_cycles | avg_i_hit_pct | avg_d_hit_pct | avg_speedup_vs_nocache |
| --- | ---: | ---: | ---: | ---: | ---: |
| wb_wa | 3/3 | 750342.67 | 99.71 | 98.20 | 11.1590 |
| wb_nowa | 3/3 | 751638.67 | 99.71 | 93.95 | 11.1440 |
| wt_wa | 3/3 | 751638.67 | 99.71 | 93.95 | 11.1440 |
| wt_nowa | 3/3 | 751638.67 | 99.71 | 93.95 | 11.1440 |
| nocache | 3/3 | 9409309.00 | 0.00 | 0.00 | 1.0000 |

- Gate status: PASS (baseline `wb_wa`, issues 0)
- Artifacts: `docs/cache_matrix/20260413/benchmark_policy_summary.csv`, `docs/cache_matrix/20260413/benchmark_cache_gate_result.json`, `docs/cache_matrix/20260413/benchmark_cache_gate_report.md`

### Local Gate (Blocking)

```bash
# full pipeline: build + ctest + cache matrix + cache gate + benchmark gate + benchmark cache gate
./tools/run_cache_gate_local.sh --run-tag 20260414

# reuse existing summaries for quick gate check
./tools/run_cache_gate_local.sh \
  --skip-build --skip-ctest --skip-matrix --skip-benchmark --skip-benchmark-matrix \
  --summary docs/cache_matrix/20260413/policy_summary.csv \
  --bench-summary docs/benchmark/20260414/benchmark_summary.csv \
  --bench-matrix-summary docs/cache_matrix/20260413/benchmark_policy_summary.csv
```

## Cache Regression Matrix And Gate

```bash
# 1) Run 4 cache policy combinations + no-cache baseline
./tools/run_cache_matrix.sh --run-tag 20260413

# 2) Evaluate PASS/WARN/FAIL gate based on policy summary
python3 tools/check_cache_gate.py \
  --summary docs/cache_matrix/20260413/policy_summary.csv \
  --baseline wb_wa \
  --output-prefix docs/cache_matrix/20260413/gate

# 3) Regenerate full report with matrix/gate sections
python3 tools/gen_full_test_report.py \
  --run-tag 20260414 \
  --run-dir tmp/full_run_20260413 \
  --cache-matrix-dir docs/cache_matrix/20260413 \
  --gate-prefix docs/cache_matrix/20260413/gate \
  --benchmark-dir docs/benchmark/20260414 \
  --benchmark-gate-prefix docs/benchmark/20260414/benchmark_gate
```

### CI Entry (ready-to-enable)

- Workflow file: `.github/workflows/cache-gate.yml`
- Scope: cache matrix gate + benchmark gate + benchmark cache gate (all support non-zero blocking)
- Current trigger: `workflow_dispatch` (manual)
- To enable automatic blocking on PR/push later, uncomment `push/pull_request` triggers in the workflow file.

### Matrix/Gate Outputs

- `docs/cache_matrix/<run-tag>/policy_summary.csv`
- `docs/cache_matrix/<run-tag>/matrix_detail.csv`
- `docs/cache_matrix/<run-tag>/gate_checks.csv`
- `docs/cache_matrix/<run-tag>/gate_result.json`
- `docs/cache_matrix/<run-tag>/gate_report.md`
- `docs/cache_matrix/<run-tag>/benchmark_policy_summary.csv`
- `docs/cache_matrix/<run-tag>/benchmark_matrix_detail.csv`
- `docs/cache_matrix/<run-tag>/benchmark_cache_gate_checks.csv`
- `docs/cache_matrix/<run-tag>/benchmark_cache_gate_result.json`
- `docs/cache_matrix/<run-tag>/benchmark_cache_gate_report.md`
- `docs/benchmark/<run-tag>/benchmark_p1.csv`
- `docs/benchmark/<run-tag>/benchmark_p10.csv`
- `docs/benchmark/<run-tag>/benchmark_nocache.csv`
- `docs/benchmark/<run-tag>/benchmark_detail.csv`
- `docs/benchmark/<run-tag>/benchmark_summary.csv`
- `docs/benchmark/<run-tag>/benchmark_gate_checks.csv`
- `docs/benchmark/<run-tag>/benchmark_gate_result.json`
- `docs/benchmark/<run-tag>/benchmark_gate_report.md`

## Cache Highlights

- Split I-cache / D-cache with configurable size, line size, associativity and miss penalty.
- Configurable policies: write-back/write-through and write-allocate/no-write-allocate.
- Miss decomposition: cold/conflict/capacity counters are exported in perf line, CSV and trace metrics.
- Cache regression automation: WB/WT x WA/no-WA matrix + no-cache baseline.
- Gate automation: PASS/WARN/FAIL checks for functional regressions, hit-rate drops and cycle regressions.
- Latest CTest (20260413): 20/20 PASS.
- Latest matrix (20260413): all 5 profiles are 42/42 PASS.
- Latest gate (20260413): PASS with baseline `wb_wa` and 0 issues.
- Latest benchmark gate (20260414): PASS with 0 issues.
- Latest benchmark cache gate (20260413): PASS with 0 issues.
- Benchmark snapshot (p10 vs no-cache, 20260413):
  - hello cycles: 161 vs 1518 (9.43x)
  - matmul cycles: 277966 vs 3151861 (11.34x)
  - quicksort_stress cycles: 1972901 vs 25074548 (12.71x)
- Benchmark cache matrix highlight (20260413):
  - wb_wa: avg_cycles 750342.67, avg_d_hit_pct 98.20, avg_speedup_vs_nocache 11.1590x
  - nocache: avg_cycles 9409309.00, avg_d_hit_pct 0.00, avg_speedup_vs_nocache 1.0000x
- Latest web smoke sample (20260413): PASS (`web_health.json` + `web_index_head.txt`).
- Latest matrix winner: `wb_wa` avg speedup vs no-cache = 6.4705x.
- Verified correctness: rv32ui p1/p10/no-cache are all 42/42 PASS.
- Quantified gains (full report):
  - rv32ui p10 average speedup: 6.46x
  - p10/p1 average cycle ratio: 1.54x
  - benchmark p10 average speedup: 11.16x
  - matmul no-cache/cache: 11.34x
  - quicksort no-cache/cache: 12.71x

## Trace Dashboard

```bash
chmod +x tools/run_trace_demo.sh
./tools/run_trace_demo.sh

# optional: custom ELF and max cycles
./tools/run_trace_demo.sh benchmarks/hello.elf 200

# optional: force a fixed port
TRACE_PORT=8080 ./tools/run_trace_demo.sh benchmarks/hello.elf 200

# web smoke resampling for report
./tools/sample_web_smoke.sh --run-tag 20260413 --run-dir tmp/full_run_20260413
```

- The launcher prints the final URL to open in browser.
- If port 8080 is occupied, the launcher auto-falls back (for example 18080/18081).
- The page auto-connects to SSE on load; Connect SSE is still available for manual reconnect.
- Health check should use the printed port, for example: `curl -s http://127.0.0.1:18080/health`
- For report refresh, run `tools/sample_web_smoke.sh` first so `web_health.json` and `web_index_head.txt` are real samples.
- See also: `web/README.md`

## Repository Layout

```text
camycpu/
├── benchmarks/                # hello/matmul/quicksort_stress + linker/startup
├── docs/                      # current deliverables: reports, full csv, figures
├── archive/                   # stage archives and historical docs/csv
├── riscv-tests/               # git submodule (official tests)
├── src/                       # simulator source
├── tests/                     # ctest targets (20)
├── tools/                     # report generator + trace server scripts
├── web/                       # dashboard frontend
├── tmp/                       # temporary logs and intermediate files
├── build/                     # rebuildable artifacts
├── Testing/                   # rebuildable ctest artifacts
├── CMakeLists.txt
├── README.md
└── test_all.sh
```

## Key Files

- `tools/gen_full_test_report.py`: generates markdown report, summary CSV, and PNG charts
- `tools/run_cache_gate_local.sh`: one-command local gate pipeline (non-zero exit blocks)
- `tools/run_benchmark_profiles.sh`: benchmark p1/p10/no-cache batch runner and CSV emitter
- `tools/check_benchmark_gate.py`: benchmark PASS/WARN/FAIL gate checker
- `tools/run_benchmark_cache_matrix.sh`: benchmark cache strategy matrix runner
- `tools/check_benchmark_cache_gate.py`: benchmark cache matrix PASS/WARN/FAIL gate checker
- `tools/sample_web_smoke.sh`: trace_server web smoke sampler for report inputs
- `docs/FULL_TEST_PERFORMANCE_REPORT.md`: consolidated full-run report
- `docs/rv32ui_perf_full_p1.csv`: rv32ui profile p1 dataset
- `docs/rv32ui_perf_full_p10.csv`: rv32ui profile p10 dataset
- `docs/rv32ui_perf_full_nocache.csv`: rv32ui profile no-cache dataset
- `docs/DOCS_CATALOG.md`: docs md/csv classification and purpose index
- `.github/workflows/cache-gate.yml`: CI workflow template for gate blocking
- `web/index.html`, `web/app.js`, `web/style.css`: trace dashboard

## Docs Organization

- `docs/` keeps current delivery artifacts only:
  - `FULL_TEST_PERFORMANCE_REPORT.md`
  - `BENCHMARK_REPORT.md`
  - `full_test_summary_20260414.csv`
  - `rv32ui_perf_full_*.csv`
  - `figures/full_run_20260414_*.png`
  - `cache_matrix/20260413/`
  - `benchmark/20260414/`
  - `second/`
- `archive/docs-history/csv-legacy/full_test_summary_20260413.csv` keeps the previous full summary snapshot.
- `archive/docs-history/figures/full_run_20260413_*.png` keeps the previous figure snapshot.
- `archive/docs-history/reports/` keeps historical markdown reports.
- `archive/docs-history/csv-legacy/` keeps historical csv datasets.
- `archive/docs-history/figures/` keeps historical figure snapshots.
- See `docs/DOCS_CATALOG.md` for file-by-file explanations.

## docs/second Deliverables

- `docs/second/README.md`: second-stage document index.
- `docs/second/progress_audit_20260413.md`: progress audit and cache-mainline status.
- `docs/second/cache_gate_usage.md`: cache gate usage, threshold tuning, and CI example.
- `docs/second/verification_checklist_20260413.md`: final verification checklist.
- `docs/second/visual_demo_guide.md`: on-site visual demo guide for stall animation and cache hit-rate.
- `docs/second/detection_report_20260413.md`: consolidated detection report.

## Archive And Cleanup

- Archive stage outputs under `archive/` with stage folders (`stage-01` ... `stage-05`) and `MANIFEST.md`
- Safe to delete and rebuild:
  - `build/`
  - `Testing/`
  - `tmp/full_run_*/` (after final docs artifacts are confirmed)
- Keep final docs artifacts in `docs/`
