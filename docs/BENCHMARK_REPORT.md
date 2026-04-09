# Benchmark Report — 16x16 整数矩阵乘法（复测 2026-04-09）

说明：为满足演示时效性，基准规模调整为 `16 x 16`，并在 Release/O3 + `--quiet` 条件下进行完整运行（不再使用 `--cycles` 限制）。

本次复测状态：cache/no-cache 两次运行均返回 `rc=0`。

环境与命令：

- 构建：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
- 生成基准 ELF：
  - `riscv64-unknown-elf-gcc -march=rv32i -mabi=ilp32 -O3 -ffreestanding -nostdlib -nostartfiles -Wl,-T,benchmarks/linker.ld benchmarks/start.S benchmarks/matmul.c -o benchmarks/matmul.elf`
- 测量（带 cache，4-way，miss penalty=10）：
  - `./build/mycpu --quiet --load benchmarks/matmul.elf --cache-penalty 10`
- 测量（无 cache，惩罚=10）：
  - `./build/mycpu --quiet --load benchmarks/matmul.elf --no-cache --cache-penalty 10`
- 本次原始日志（本轮重新执行）：
  - `tmp/full_run_20260409/matmul_cache_p10.log`
  - `tmp/full_run_20260409/matmul_nocache_p10.log`
  - `tmp/full_run_20260409/matmul_recheck_rcs.csv`

原始输出（完整运行）：

```
Benchmark Finished
37287936
Cycles: 277966, Instrs: 230400, I-Cache Hit: 99.98%, D-Cache Hit: 99.50%

Benchmark Finished
37287936
Cycles: 3151861, Instrs: 230400, I-Cache Hit: 0.00%, D-Cache Hit: 0.00%
```

对比结果：

- 加速比（no-cache / cache）: `3151861 / 277966 = 11.34x`
- IPC（cache）: `230400 / 277966 = 0.8289`
- IPC（no-cache）: `230400 / 3151861 = 0.0731`

结论：在 `16x16` 规模下，开启 cache 后总周期显著降低，观测到约 **11.34x** 的性能提升；两次运行 checksum 一致（`37287936`），结果正确性一致。本次复测与之前测量值一致。
