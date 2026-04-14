# Cache Gate 使用说明

本文档说明如何在本地执行 cache gate + benchmark gate、如何调节阈值，以及后续接入 CI 的推荐做法。

## 1. 本地一键门禁（阻断模式）

```bash
cd /home/ys/camycpu
./tools/run_cache_gate_local.sh --run-tag 20260413
```

默认流程：
1. configure + build
2. ctest
3. run_cache_matrix.sh（5 组策略）
4. check_cache_gate.py（判定 PASS/WARN/FAIL）
5. run_benchmark_profiles.sh（benchmark 三配置跑数）
6. check_benchmark_gate.py（benchmark PASS/WARN/FAIL）

退出码策略：
- PASS -> 0
- WARN -> 1（默认阻断）
- FAIL -> 2（阻断）

如果你希望 WARN 不阻断：

```bash
./tools/run_cache_gate_local.sh --run-tag 20260413 --allow-warn
```

## 2. 快速复用已有矩阵结果

当 `policy_summary.csv` 与 `benchmark_summary.csv` 已存在时，可跳过 build/ctest/matrix/benchmark：

```bash
./tools/run_cache_gate_local.sh \
  --skip-build --skip-ctest --skip-matrix --skip-benchmark \
  --summary docs/cache_matrix/20260413/policy_summary.csv \
  --bench-summary docs/benchmark/20260414/benchmark_summary.csv
```

## 3. 阈值定义

门禁核心指标定义如下：

- `cycles_regress_pct = (policy_avg_cycles - baseline_avg_cycles) / baseline_avg_cycles * 100%`
- `i_hit_drop_pct = baseline_avg_i_hit_pct - policy_avg_i_hit_pct`
- `d_hit_drop_pct = baseline_avg_d_hit_pct - policy_avg_d_hit_pct`
- `cache_stall_ratio = avg_cache_stall / avg_stall`

说明：
- `nocache` 只作为功能正确性与速度基线参考，不参与命中率/周期回退阈值比较。
- baseline 默认 `wb_wa`，可通过 `--baseline` 调整。

## 4. 阈值调参建议

### 4.1 推荐档位

| 场景 | max-cycle-regress-pct | max-ihit-drop-pct | max-dhit-drop-pct | min-cache-stall-ratio |
|---|---:|---:|---:|---:|
| 本地开发（宽松） | 20.0 | 8.0 | 10.0 | 0.15 |
| 预提交（中等） | 15.0 | 5.0 | 8.0 | 0.20 |
| CI正式门禁（严格） | 12.0 | 4.0 | 6.0 | 0.20 |

### 4.2 命令示例

```bash
python3 tools/check_cache_gate.py \
  --summary docs/cache_matrix/20260413/policy_summary.csv \
  --baseline wb_wa \
  --max-cycle-regress-pct 12 \
  --max-ihit-drop-pct 4 \
  --max-dhit-drop-pct 6 \
  --min-cache-stall-ratio 0.20 \
  --output-prefix docs/cache_matrix/20260413/gate
```

### 4.3 benchmark gate 阈值

| 场景 | fail-min-speedup-p10 | warn-min-speedup-p10 | fail-max-penalty-ratio | warn-max-penalty-ratio |
|---|---:|---:|---:|---:|
| 本地开发（宽松） | 1.00 | 1.20 | 4.00 | 3.20 |
| 预提交（中等） | 1.20 | 1.50 | 3.00 | 2.50 |
| CI正式门禁（严格） | 1.30 | 1.60 | 2.80 | 2.30 |

命令示例：

```bash
python3 tools/check_benchmark_gate.py \
  --summary docs/benchmark/20260414/benchmark_summary.csv \
  --fail-min-speedup-p10 1.20 \
  --warn-min-speedup-p10 1.50 \
  --fail-max-penalty-ratio 3.00 \
  --warn-max-penalty-ratio 2.50 \
  --output-prefix docs/benchmark/20260414/benchmark_gate
```

## 5. CI 示例（GitHub Actions）

仓库已提供 workflow 模板：
- `.github/workflows/cache-gate.yml`

当前触发方式为 `workflow_dispatch`（手动触发）。
如需切到自动门禁，可在该文件中打开 `push/pull_request` 触发器。

关键步骤示例：

```yaml
- name: Run cache gate pipeline
  run: |
    chmod +x tools/run_cache_gate_local.sh tools/run_cache_matrix.sh
    ./tools/run_cache_gate_local.sh \
      --run-tag "$TAG_INPUT" \
      --cycles "$CYCLES_INPUT"
```

由于脚本使用非零退出码表达 WARN/FAIL，CI 会自动阻断。

说明：`run_cache_gate_local.sh` 当前默认会执行 cache gate + benchmark gate 两段判定。

## 6. 产物位置

一次门禁执行后的关键产物：

- `docs/cache_matrix/<run-tag>/policy_summary.csv`
- `docs/cache_matrix/<run-tag>/matrix_detail.csv`
- `docs/cache_matrix/<run-tag>/gate_checks.csv`
- `docs/cache_matrix/<run-tag>/gate_result.json`
- `docs/cache_matrix/<run-tag>/gate_report.md`
- `docs/benchmark/<run-tag>/benchmark_p1.csv`
- `docs/benchmark/<run-tag>/benchmark_p10.csv`
- `docs/benchmark/<run-tag>/benchmark_nocache.csv`
- `docs/benchmark/<run-tag>/benchmark_detail.csv`
- `docs/benchmark/<run-tag>/benchmark_summary.csv`
- `docs/benchmark/<run-tag>/benchmark_gate_checks.csv`
- `docs/benchmark/<run-tag>/benchmark_gate_result.json`
- `docs/benchmark/<run-tag>/benchmark_gate_report.md`

## 7. 常见问题

1. 运行耗时较长
- 5 策略 x 42 测试是正常开销。
- 可先用 `--skip-*` 模式复用已有数据进行快速判定。

2. gate 报告是 FAIL 但功能都通过
- 先看 `gate_checks.csv` 的 `metric` 与 `threshold`。
- 若确认是设备性能差异引起，可适度上调阈值，不要直接关闭门禁。

3. 结果文件被覆盖
- 请使用唯一 `--run-tag`，建议格式 `YYYYMMDD[-suffix]`。
