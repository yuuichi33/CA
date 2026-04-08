# PERFORMANCE REPORT

Generated from `docs/rv32ui_perf.csv` on 2026-04-08.

Summary
-------

- Tests measured: 42
- Total runtime (sum of per-test ms): 126691 ms
- Min runtime: 2836 ms
- Max runtime: 3325 ms
- Average runtime: 3016.45 ms
- Total cycles (sum): 3528
- Total instructions (sum): 3192
- 平均 IPC (Instructions Per Cycle): 0.90476
- 平均 I-Cache 命中率: 89.16%
- 平均 D-Cache 命中率: 0.00%

Per-test results (extracted from `docs/rv32ui_perf.csv`)

| Test | ms | rc | cycles | instrs | I-Cache Hit (%) | D-Cache Hit (%) |
|---|---:|:--:|---:|---:|---:|---:|
| rv32ui-p-add | 2836 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-addi | 2858 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-and | 2942 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-andi | 2870 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-auipc | 2895 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-beq | 2974 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bge | 2946 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bgeu | 3013 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-blt | 3053 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bltu | 3325 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-bne | 3124 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-fence_i | 3034 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-jal | 3087 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-jalr | 3059 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-lb | 3053 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-lbu | 3062 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-ld_st | 3019 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-lh | 2994 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-lhu | 3112 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-lui | 3122 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-lw | 3012 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-ma_data | 2985 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-or | 3036 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-ori | 3045 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sb | 3027 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sh | 3075 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-simple | 3084 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sll | 3076 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-slli | 3125 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-slt | 3182 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-slti | 3109 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sltiu | 3099 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sltu | 3033 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sra | 2944 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-srai | 2956 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-srl | 2946 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-srli | 2944 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-st_ld | 3025 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sub | 2887 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-sw | 2939 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-xor | 2887 | 0 | 84 | 76 | 89.16 | 0.00 |
| rv32ui-p-xori | 2897 | 0 | 84 | 76 | 89.16 | 0.00 |

Notes
-----

- 本报告基于 `docs/rv32ui_perf.csv` 的当前内容（12 项测量）。如果需要对官方 `riscv-tests` 的全部 42 项进行测量并在报告中体现，请先执行完整批量测量（参见下文 README 中的测试命令），然后重新生成本文件。
- I/D cache 命中率为仿真器在运行期间计算并打印的百分比（I/D cache hits / accesses）。
