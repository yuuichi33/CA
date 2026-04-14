# Repository Index（2026-04-14）

本文档用于快速定位仓库根目录及核心子目录。

## 1. 根目录总览

| 路径 | 角色 | 状态 |
|---|---|---|
| `src/` | CPU/Cache/MMU/外设核心实现 | 当前 |
| `tests/` | CTest 测试程序 | 当前 |
| `tools/` | 自动化脚本（report/matrix/gate/trace） | 当前 |
| `docs/` | 当前交付文档与数据 | 当前 |
| `archive/` | 历史产物与阶段归档 | 历史 |
| `tmp/` | 运行中间日志与临时产物 | 临时 |
| `web/` | 可视化页面 | 当前 |
| `benchmarks/` | benchmark ELF 与源文件 | 当前 |
| `riscv-tests/` | 官方测试子模块 | 当前 |

## 2. 当前交付入口

- 主报告：`docs/FULL_TEST_PERFORMANCE_REPORT.md`
- 当前数据：
  - `docs/rv32ui_perf_full_p1.csv`
  - `docs/rv32ui_perf_full_p10.csv`
  - `docs/rv32ui_perf_full_nocache.csv`
  - `docs/full_test_summary_20260414.csv`
- Cache 门禁：`docs/cache_matrix/20260413/`
- Benchmark 数据与门禁：`docs/benchmark/20260414/`
- Benchmark cache matrix 与门禁：`docs/cache_matrix/20260413/benchmark_policy_summary.csv` + `benchmark_cache_gate_result.json`
- Web smoke 重采样：`tmp/full_run_20260413/web_health.json` + `web_index_head.txt`
- 汇报资料：`docs/second/`

## 3. 历史归档入口

- `archive/docs-history/reports/`
- `archive/docs-history/csv-legacy/`
- `archive/docs-history/figures/`
- `archive/stage-01-rv32ui-collect/` ... `archive/stage-05-legacy-scripts/`

已迁移快照：
- `archive/docs-history/csv-legacy/full_test_summary_20260413.csv`
- `archive/docs-history/figures/full_run_20260413_*.png`

## 4. 快速导航建议

1. 想看当前结果：优先看 `docs/` 与 `docs/second/`。
2. 想追溯历史：看 `archive/` 与 `archive/MANIFEST.md`。
3. 想跑门禁：看 `tools/run_cache_gate_local.sh` 和 `docs/second/cache_gate_usage.md`。
