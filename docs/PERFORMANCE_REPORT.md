# PERFORMANCE REPORT

Generated from `docs/rv32ui_perf.csv` on 2026-04-08.

Summary
-------

- Tests measured: 12
- Total runtime (sum of per-test ms): 35369 ms
- Min runtime: 2758 ms
- Max runtime: 3081 ms
- Average runtime: 2947.42 ms
- Total cycles (sum): 1008
- Total instructions (sum): 912
- 平均 IPC (Instructions Per Cycle): 0.90476
- 平均 I-Cache 命中率: 89.16%
- 平均 D-Cache 命中率: 0.00%

Per-test results (extracted from `docs/rv32ui_perf.csv`)

| Test | ms | rc | cycles | instrs | I-Cache Hit (%) | D-Cache Hit (%) |
|---|---:|:--:|---:|---:|---:|---:|
| rv32ui-p-add | 2934 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-addi | 2759 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-and | 3003 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-andi | 2758 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-auipc | 2919 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-beq | 2838 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bge | 3021 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bgeu | 3026 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-blt | 3056 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bltu | 2982 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bne | 2992 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-fence_i | 3081 | 0 | 84 | 76 | 89.16 | 0.00 |

Notes
-----

- 本报告基于 `docs/rv32ui_perf.csv` 的当前内容（12 项测量）。如果需要对官方 `riscv-tests` 的全部 42 项进行测量并在报告中体现，请先执行完整批量测量（参见下文 README 中的测试命令），然后重新生成本文件。
- I/D cache 命中率为仿真器在运行期间计算并打印的百分比（I/D cache hits / accesses）。
