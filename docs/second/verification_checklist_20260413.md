# 验收检测清单（2026-04-13）

本文档用于最终交付前的快速核对。

## 1. 构建与单元测试

- [ ] 构建通过
  - 命令：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
- [ ] CTest 全量通过
  - 命令：`ctest --test-dir build --output-on-failure`
  - 期望：20/20 PASS

## 2. RV32UI 三配置

- [ ] p1 生成并通过
  - 命令：`bash test_all.sh --csv docs/rv32ui_perf_full_p1.csv --cache-penalty 1 --quiet`
  - 期望：42/42 PASS
- [ ] p10 生成并通过
  - 命令：`bash test_all.sh --csv docs/rv32ui_perf_full_p10.csv --cache-penalty 10 --quiet`
  - 期望：42/42 PASS
- [ ] no-cache 生成并通过
  - 命令：`bash test_all.sh --csv docs/rv32ui_perf_full_nocache.csv --no-cache --cache-penalty 10 --quiet`
  - 期望：42/42 PASS

## 3. Cache Matrix 与 Gate

- [ ] matrix 五策略输出完整
  - 命令：`./tools/run_cache_matrix.sh --run-tag <tag>`
  - 期望：`policy_summary.csv` 与 `matrix_detail.csv` 存在
- [ ] gate 判定通过
  - 命令：`python3 tools/check_cache_gate.py --summary docs/cache_matrix/<tag>/policy_summary.csv --baseline wb_wa --output-prefix docs/cache_matrix/<tag>/gate`
  - 期望：`gate_result.json` 中 `status=PASS`
- [ ] 本地阻断入口有效
  - 命令：`./tools/run_cache_gate_local.sh --run-tag <tag>`
  - 期望：WARN/FAIL 返回非零，PASS 返回 0

## 4. 报告与图表

- [ ] FULL 报告更新
  - 文件：`docs/FULL_TEST_PERFORMANCE_REPORT.md`
- [ ] 汇总 CSV 更新
  - 文件：`docs/full_test_summary_<tag>.csv`
- [ ] 3 张图表更新
  - 文件：`docs/figures/full_run_<tag>_*.png`

## 5. 文档一致性

- [ ] README 与 FULL 报告核心数字一致
- [ ] second 目录文档齐全
  - `progress_audit_20260413.md`
  - `cache_gate_usage.md`
  - `verification_checklist_20260413.md`
  - `visual_demo_guide.md`
  - `detection_report_20260413.md`
- [ ] docs 索引与 archive 清单已同步
  - `docs/DOCS_CATALOG.md`
  - `archive/MANIFEST.md`

## 6. 归档核对

- [ ] docs 仅保留当前版本（20260413）主要产物
- [ ] 旧版（20260409）summary 与图表已归档到 `archive/docs-history/`
- [ ] 历史链接可追溯，不存在断链
