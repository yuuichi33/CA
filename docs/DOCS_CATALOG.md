# Docs Catalog

This catalog explains what each markdown/csv/json/figure artifact is for and where it lives.

## 1. Current Delivery Artifacts (kept in docs)

### 1.1 Reports and index

| File | Type | Purpose |
|---|---|---|
| `FULL_TEST_PERFORMANCE_REPORT.md` | report | Final full-matrix test and performance report (ctest + rv32ui + benchmark + web smoke + cache matrix/gate). |
| `BENCHMARK_REPORT.md` | report | Focused benchmark report for matmul/quicksort style cases. |
| `REPO_INDEX.md` | index | Root/subdirectory index for quick repository navigation. |
| `second/README.md` | index | Stage-2 presentation and verification document index. |

### 1.1.1 docs/second delivery set

- `second/progress_audit_20260413.md`
- `second/cache_gate_usage.md`
- `second/verification_checklist_20260413.md`
- `second/visual_demo_guide.md`
- `second/detection_report_20260413.md`

### 1.2 Current CSV datasets

| File | Type | Purpose |
|---|---|---|
| `full_test_summary_20260414.csv` | csv | Normalized per-test summary generated from current full report run. |
| `rv32ui_perf_full_p1.csv` | csv | rv32ui results with cache enabled and penalty=1. |
| `rv32ui_perf_full_p10.csv` | csv | rv32ui results with cache enabled and penalty=10. |
| `rv32ui_perf_full_nocache.csv` | csv | rv32ui results with cache disabled baseline. |

### 1.2.1 Current benchmark datasets

Current benchmark folder: `benchmark/20260414/`

- `benchmark_p1.csv`
- `benchmark_p10.csv`
- `benchmark_nocache.csv`
- `benchmark_detail.csv`
- `benchmark_summary.csv`
- `benchmark_gate_checks.csv`
- `benchmark_gate_result.json`
- `benchmark_gate_report.md`

### 1.3 Current figures

| File | Type | Purpose |
|---|---|---|
| `figures/full_run_20260414_speedup_bar.png` | figure | Distribution of rv32ui speedup under p10 vs no-cache. |
| `figures/full_run_20260414_hitrate_scatter.png` | figure | Correlation view between hit rate and speedup. |
| `figures/full_run_20260414_benchmark_cycles_log.png` | figure | Benchmark cycle comparison on log scale. |

### 1.4 Cache matrix and gate outputs

Current baseline folder: `cache_matrix/20260413/`

- `rv32ui_wb_wa.csv`
- `rv32ui_wb_nowa.csv`
- `rv32ui_wt_wa.csv`
- `rv32ui_wt_nowa.csv`
- `rv32ui_nocache.csv`
- `policy_summary.csv`
- `matrix_detail.csv`
- `gate_checks.csv`
- `gate_result.json`
- `gate_report.md`
- `benchmark_wb_wa.csv`
- `benchmark_wb_nowa.csv`
- `benchmark_wt_wa.csv`
- `benchmark_wt_nowa.csv`
- `benchmark_nocache.csv`
- `benchmark_policy_summary.csv`
- `benchmark_matrix_detail.csv`
- `benchmark_cache_gate_checks.csv`
- `benchmark_cache_gate_result.json`
- `benchmark_cache_gate_report.md`

## 2. Historical Artifacts (moved to archive)

### 2.1 Historical reports

- `archive/docs-history/reports/SPEEDUP_REPORT.md`
- `archive/docs-history/reports/P2_CLOSURE_REPORT.md`

### 2.2 Historical legacy CSV

- `archive/docs-history/csv-legacy/rv32ui_perf.csv`
- `archive/docs-history/csv-legacy/rv32ui_perf_p1.csv`
- `archive/docs-history/csv-legacy/rv32ui_perf_p10.csv`
- `archive/docs-history/csv-legacy/rv32ui_perf_nocache.csv`
- `archive/docs-history/csv-legacy/full_test_summary_20260409.csv`
- `archive/docs-history/csv-legacy/full_test_summary_20260413.csv`

### 2.3 Historical figures

- `archive/docs-history/figures/full_run_20260409_speedup_bar.png`
- `archive/docs-history/figures/full_run_20260409_hitrate_scatter.png`
- `archive/docs-history/figures/full_run_20260409_benchmark_cycles_log.png`
- `archive/docs-history/figures/full_run_20260413_speedup_bar.png`
- `archive/docs-history/figures/full_run_20260413_hitrate_scatter.png`
- `archive/docs-history/figures/full_run_20260413_benchmark_cycles_log.png`

## 3. Version Snapshot Map

| Version tag | Status | Main summary | Figure set | Cache matrix |
|---|---|---|---|---|
| `20260414` | current | `docs/full_test_summary_20260414.csv` | `docs/figures/full_run_20260414_*.png` | `docs/cache_matrix/20260413/` |
| `20260413` | historical (summary/figures archived) | `archive/docs-history/csv-legacy/full_test_summary_20260413.csv` | `archive/docs-history/figures/full_run_20260413_*.png` | `docs/cache_matrix/20260413/` |
| `20260409` | historical | `archive/docs-history/csv-legacy/full_test_summary_20260409.csv` | `archive/docs-history/figures/full_run_20260409_*.png` | stage baseline logs in `archive/stage-04-full-run-baseline/` |

## 4. Stage Archive (root archive)

Stage-by-stage process artifacts are kept in:

- `archive/stage-01-rv32ui-collect/`
- `archive/stage-02-benchmark-explore/`
- `archive/stage-03-failure-debug/`
- `archive/stage-04-full-run-baseline/`
- `archive/stage-05-legacy-scripts/`
- `archive/MANIFEST.md`
