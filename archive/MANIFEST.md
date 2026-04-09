# Archive Manifest (2026-04-09)

## Purpose
Archive intermediate and stage outputs before final cleanup and full re-run.

This archive lives at repository root (`archive/`) and no longer keeps an extra `phase-20260409/` wrapper directory.

## Stage Mapping

- Stage 01: RV32UI collection and compare runs
  - Source path: tmp/
  - Files moved to: stage-01-rv32ui-collect/
  - Typical origin commands: bash test_all.sh ... and collection helpers

- Stage 02: Benchmark exploration and quicksort/matmul tuning runs
  - Source path: tmp/
  - Files moved to: stage-02-benchmark-explore/
  - Typical origin commands: ./build/mycpu --load benchmarks/*.elf ...

- Stage 03: Failure debug snapshots
  - Source path: tmp/fail_debug/*.out
  - Files moved to: stage-03-failure-debug/
  - Purpose: preserve before/after fix traces for root-cause auditing

- Stage 04: Baseline full-matrix run outputs (pre-regeneration)
  - Source path: tmp/full_run_20260409/
  - Files moved to: stage-04-full-run-baseline/
  - Purpose: historical baseline before current final rerun

- Stage 05: Legacy helper scripts and one-off tooling
  - Source path: tmp/*.py, tmp/*.sh
  - Files moved to: stage-05-legacy-scripts/

- Docs history: legacy markdown/csv moved out of `docs/`
  - Files moved to: docs-history/reports/ and docs-history/csv-legacy/
  - Purpose: keep `docs/` focused on current delivery artifacts

## Notes
- This archive stores stage provenance only; final deliverables are kept in docs/ root and docs/figures/.
- Reproducible command set is documented in README and FULL_TEST_PERFORMANCE_REPORT.
