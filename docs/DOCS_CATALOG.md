# Docs Catalog

This catalog explains what each markdown/csv artifact is for and where it lives.

## 1. Current Delivery Artifacts (kept in docs)

| File | Type | Purpose |
|---|---|---|
| `FULL_TEST_PERFORMANCE_REPORT.md` | report | Final full-matrix test + performance report (ctest + rv32ui + benchmark + web smoke). |
| `BENCHMARK_REPORT.md` | report | Focused benchmark report for 16x16 matmul cache-on vs no-cache. |
| `full_test_summary_20260409.csv` | csv | Normalized per-test summary generated with the final full report. |
| `rv32ui_perf_full_p1.csv` | csv | rv32ui results with cache enabled and penalty=1. |
| `rv32ui_perf_full_p10.csv` | csv | rv32ui results with cache enabled and penalty=10. |
| `rv32ui_perf_full_nocache.csv` | csv | rv32ui results with cache disabled baseline. |
| `figures/full_run_20260409_speedup_bar.png` | figure | Distribution of rv32ui speedup under p10 vs no-cache. |
| `figures/full_run_20260409_hitrate_scatter.png` | figure | Correlation view between hit rate and speedup. |
| `figures/full_run_20260409_benchmark_cycles_log.png` | figure | Benchmark cycle comparison on log scale. |

## 2. Historical Reports (moved to archive)

These files are still useful for traceability but are no longer primary outputs:

- `archive/docs-history/reports/SPEEDUP_REPORT.md`
- `archive/docs-history/reports/P2_CLOSURE_REPORT.md`

## 3. Historical Legacy CSV (moved to archive)

These are legacy/earlier snapshots superseded by `rv32ui_perf_full_*`:

- `archive/docs-history/csv-legacy/rv32ui_perf.csv`
- `archive/docs-history/csv-legacy/rv32ui_perf_p1.csv`
- `archive/docs-history/csv-legacy/rv32ui_perf_p10.csv`
- `archive/docs-history/csv-legacy/rv32ui_perf_nocache.csv`

## 4. Stage Archive (root archive)

Stage-by-stage process artifacts are kept in:

- `archive/stage-01-rv32ui-collect/`
- `archive/stage-02-benchmark-explore/`
- `archive/stage-03-failure-debug/`
- `archive/stage-04-full-run-baseline/`
- `archive/stage-05-legacy-scripts/`
- `archive/MANIFEST.md`
