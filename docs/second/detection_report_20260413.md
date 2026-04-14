# Detection Report（2026-04-13，更新于 2026-04-14）

## 1. 目标与范围

本报告汇总本轮交付的检测结果，覆盖：
- build + ctest
- rv32ui 三配置
- cache matrix + gate
- benchmark 三配置 + benchmark gate
- FULL 报告一致性

## 2. 检测结论

- 总结论：PASS
- 门禁状态：PASS（baseline=wb_wa，issues=0）
- 当前建议：可用于课程演示与阶段答辩

## 3. 关键结果

### 3.1 ctest

- 结果：20/20 PASS
- 命令：`ctest --test-dir build --output-on-failure`

### 3.2 rv32ui 三配置

- p1：42/42 PASS
- p10：42/42 PASS
- no-cache：42/42 PASS

数据文件：
- `docs/rv32ui_perf_full_p1.csv`
- `docs/rv32ui_perf_full_p10.csv`
- `docs/rv32ui_perf_full_nocache.csv`

### 3.3 cache matrix（20260413）

| policy | pass/tests | avg_cycles | avg_i_hit_pct | avg_d_hit_pct | avg_speedup_vs_nocache |
|---|---:|---:|---:|---:|---:|
| wb_wa | 42/42 | 696.88 | 93.50 | 19.41 | 6.4705 |
| wb_nowa | 42/42 | 698.88 | 93.50 | 18.34 | 6.4553 |
| wt_wa | 42/42 | 698.88 | 93.50 | 18.34 | 6.4553 |
| wt_nowa | 42/42 | 698.88 | 93.50 | 18.34 | 6.4553 |
| nocache | 42/42 | 4633.83 | 0.00 | 0.00 | 1.0000 |

### 3.4 gate 结果

- 状态：PASS
- issues：0
- 结果文件：
  - `docs/cache_matrix/20260413/gate_result.json`
  - `docs/cache_matrix/20260413/gate_report.md`
  - `docs/cache_matrix/20260413/gate_checks.csv`

### 3.5 benchmark 三配置（20260414）

| benchmark | rc_p1 | rc_p10 | rc_nocache | cycles_p10 | cycles_nocache | speedup_p10 |
|---|---:|---:|---:|---:|---:|---:|
| hello | 0 | 0 | 0 | 161 | 1518 | 9.43x |
| matmul | 0 | 0 | 0 | 277966 | 3151861 | 11.34x |
| quicksort_stress | 0 | 0 | 0 | 1972901 | 25074548 | 12.71x |

- benchmark gate：PASS（issues=0）
- 结果文件：
  - `docs/benchmark/20260414/benchmark_summary.csv`
  - `docs/benchmark/20260414/benchmark_gate_result.json`
  - `docs/benchmark/20260414/benchmark_gate_report.md`

## 4. 报告一致性核对

已核对以下文档口径一致：
- `README.md`
- `docs/FULL_TEST_PERFORMANCE_REPORT.md`
- `docs/second/progress_audit_20260413.md`

核心数字（示例）：
- rv32ui p10 平均 speedup：6.46x
- p10/p1 cycle 比：1.54x
- matrix 最优策略：wb_wa（6.4705x vs no-cache）

## 5. 建议

1. 继续以 `wb_wa` 作为门禁 baseline。
2. 后续启用 CI 时，先采用 `workflow_dispatch`，稳定后再打开 PR 自动触发。
3. 若 CI 设备波动较大，可先使用中等阈值，再逐步收紧。
