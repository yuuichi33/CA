# P2 结案报告（2026-04-09）

## 范围
- 先执行第 1 项：rv32ui 全量回归，分别验证 cache-penalty=1 与 cache-penalty=10。
- 再执行第 2 项：形成 P2 修复与验证报告并落盘到 docs 目录。

## 1. rv32ui 全量回归结果

### 执行命令
- cd /home/ys/camycpu && bash test_all.sh --csv docs/rv32ui_perf_p1.csv --cache-penalty 1 --quiet | tee tmp/rv32ui_p1_run.log
- cd /home/ys/camycpu && bash test_all.sh --csv docs/rv32ui_perf_p10.csv --cache-penalty 10 --quiet | tee tmp/rv32ui_p10_run.log

### 结果摘要
- penalty=1：42/42 通过，失败 0，Pass rate 100%。
- penalty=10：42/42 通过，失败 0，Pass rate 100%。

### 产物
- CSV: [archive/docs-history/csv-legacy/rv32ui_perf_p1.csv](../csv-legacy/rv32ui_perf_p1.csv)
- CSV: [archive/docs-history/csv-legacy/rv32ui_perf_p10.csv](../csv-legacy/rv32ui_perf_p10.csv)
- 运行日志: [archive/stage-01-rv32ui-collect/rv32ui_p1_run.log](../../stage-01-rv32ui-collect/rv32ui_p1_run.log)
- 运行日志: [archive/stage-01-rv32ui-collect/rv32ui_p10_run.log](../../stage-01-rv32ui-collect/rv32ui_p10_run.log)

### 聚合统计（由 CSV 计算）
- penalty=1：tests=42, fail=0, total_ms=114362, avg_ms=2722.90, avg_cycles=462.02, avg_instrs=377.12
- penalty=10：tests=42, fail=0, total_ms=113671, avg_ms=2706.45, avg_cycles=696.88, avg_instrs=377.12

## 2. P2 问题根因与修复

### 根因
- 在 MEM load miss 导致的流水停顿期间，ID/EX 中冻结指令保留了较早拍采样到的 rs 值。
- 停顿结束后，部分分支/算术指令错过前递窗口，最终使用陈旧操作数执行，导致控制流偏移。

### 修复
- 在 EX 执行阶段重新读取 rs1/rs2，再叠加前递覆盖，避免长停顿后的陈旧操作数问题。
- 代码定位：
  - [src/cpu/pipeline.cpp](../../../src/cpu/pipeline.cpp#L603)

### 回归测试补充
- 新增了专门覆盖“load miss stall + 分支比较”路径的回归用例。
- 代码定位：
  - [tests/test_pipeline.cpp](../../../tests/test_pipeline.cpp#L19)
  - [tests/test_pipeline.cpp](../../../tests/test_pipeline.cpp#L92)

## 3. quicksort_stress 压测验证

### 运行结果
- 默认 cache 配置（开启 cache）通过：
  - [archive/stage-02-benchmark-explore/quicksort_stress_default_after_fix.log](../../stage-02-benchmark-explore/quicksort_stress_default_after_fix.log)
  - checksum=0xe48d8e25
- no-cache 基线通过：
  - [archive/stage-02-benchmark-explore/quicksort_stress_cache_off_after_fix.log](../../stage-02-benchmark-explore/quicksort_stress_cache_off_after_fix.log)
  - checksum=0xe48d8e25
- write-through 配置通过：
  - [archive/stage-02-benchmark-explore/quicksort_stress_wt_after_fix.log](../../stage-02-benchmark-explore/quicksort_stress_wt_after_fix.log)
  - checksum=0xe48d8e25

## 4. 结论
- P2 本轮目标已完成：问题已修复，新增回归已覆盖，rv32ui 在 penalty=1/10 下均保持 42/42 全通过。
- 修复后复杂工作负载 quicksort_stress 在 cache on/off 与 write-through 模式均稳定通过，未观察到本次同类回归。