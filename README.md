# myCPU — RV32I 五级流水线模拟器

基于 C++17 + CMake 的 RV32I 指令集模拟器，面向计算机体系结构教学与微架构实验。

- [结题报告](docs/second/final_report.md)
- [结题汇报](docs/second/CA_ys_260423_finalpresentation_git.pdf)

## 项目简介

本项目已完成一台可独立运行的 RV32I 教学模拟器 myCPU。实现上覆盖 49 条有效指令，完成五级流水、I/D Cache、Sv32 基础 MMU、异常与中断、UART/Timer 外设。验证上通过 20 项 CTest 与 42 项 rv32ui 三配置全量测试。性能上，cache 在 rv32ui 提供 6.46x 平均加速，在 matmul 与 quicksort 分别达到 11.34x 和 12.71x；benchmark cache matrix 显示 wb_wa 在当前样本下综合最优并已纳入门禁。


## 特性

| 模块 | 实现 |
|---|---|
| **流水线** | 五级顺序流水：IF → ID → EX → MEM → WB，含前递（Forwarding）与 load-use 停顿 |
| **指令集** | RV32I 全集 + CSR/特权指令，共 **49 条**有效指令 |
| **特权级** | M/S/U 三模式，支持 trap 委托（ECALL/MRET/SRET） |
| **I/D Cache** | 分离指令/数据 Cache，可配置容量/相联度/行大小/写策略/写分配策略，LRU 替换 |
| **MMU** | Sv32 两级页表 + ASID-aware TLB + SFENCE.VMA |
| **外设** | UART MMIO（含 semihosting tohost）、Timer MMIO（mtime 中断） |
| **ELF 加载** | 解析 RISC-V ELF，加载至模拟内存 |
| **Trace** | JSONL 输出 + SSE 服务端推送 + Web 可视化仪表盘 |
| **自动化** | Bash 测试脚本 + Python 报告生成 + 性能门禁 |


## 指令覆盖

共 **49 条**有效指令（不含 UNKNOWN/SYSTEM 占位）：

| 类别 | 数量 | 指令 |
|---|---:|---|
| R 型算术 | 10 | ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND |
| I 型算术 | 9 | ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI |
| Load | 5 | LB, LH, LW, LBU, LHU |
| Store | 3 | SB, SH, SW |
| Branch | 6 | BEQ, BNE, BLT, BGE, BLTU, BGEU |
| U 型 | 2 | LUI, AUIPC |
| JAL / JALR | 2 | JAL, JALR |
| CSR / 特权 | 10 | ECALL, MRET, SRET, SFENCE.VMA, CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI |
| Fence | 2 | FENCE, FENCE.I |


## Quick Start

### 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 运行

```bash
# 加载 ELF 并运行
./build/mycpu --load benchmarks/hello.elf

# 限制最大周期数
./build/mycpu --load benchmarks/matmul.elf --cycles 5000000

# 禁用 Cache
./build/mycpu --load benchmarks/hello.elf --no-cache --cache-penalty 10

# 自定义 Cache 参数
./build/mycpu --load benchmarks/quicksort_stress.elf \
  --cache-size 32768 --cache-line-size 64 --cache-assoc 8 \
  --cache-write-back --cache-write-allocate

# 启用 Trace 输出（用于 Web 可视化）
./build/mycpu --load benchmarks/hello.elf --trace-json stdout
```

### CLI 参数

| 参数 | 说明 |
|---|---|
| `--load <file>` / `-l` | 加载 RISC-V ELF 文件 |
| `--cycles <n>` / `-c` | 最大运行周期数 |
| `--no-cache` | 禁用 Cache |
| `--cache-penalty <n>` | Miss 延迟（默认 10） |
| `--cache-size <bytes>` | 总容量（默认 16 KB） |
| `--cache-line-size <bytes>` | 行大小（默认 64 B） |
| `--cache-assoc <n>` | 组相联度（默认 4） |
| `--cache-write-back` / `--cache-write-through` | 写策略 |
| `--cache-write-allocate` / `--cache-no-write-allocate` | 写分配策略 |
| `--trace-json <stdout\|path>` | JSONL Trace 输出 |
| `--uart-stdin` / `-u` | 启用 UART 标准输入桥接 |
| `--timer-interval <n>` / `--timer-enable` | 定时器配置 |
| `--verbose` / `-v` | 详细输出 |
| `--quiet` / `-q` | 静默模式 |
| `--step` / `-s` | 单步模式 |
| `--help` / `-h` | 查看完整帮助 |


## 测试

### 运行 CTest

```bash
ctest --test-dir build --output-on-failure
```

### 运行 rv32ui 全量测试

```bash
# p1: cache 开启, penalty=1
bash test_all.sh --csv docs/rv32ui_perf_full_p1.csv --cache-penalty 1 --quiet

# p10: cache 开启, penalty=10
bash test_all.sh --csv docs/rv32ui_perf_full_p10.csv --cache-penalty 10 --quiet

# no-cache: cache 关闭
bash test_all.sh --csv docs/rv32ui_perf_full_nocache.csv --no-cache --cache-penalty 10 --quiet
```

### Cache 策略矩阵测试

```bash
# 运行 5 种策略组合的完整矩阵
bash tools/run_cache_matrix.sh --run-tag <tag>

# 门禁检查
python3 tools/check_cache_gate.py \
  --summary docs/cache_matrix/<tag>/policy_summary.csv \
  --baseline wb_wa \
  --output-prefix docs/cache_matrix/<tag>/gate
```

### Benchmark 性能测试

```bash
bash tools/run_benchmark_profiles.sh --run-tag <tag> --cycles 50000000
python3 tools/check_benchmark_gate.py \
  --summary docs/benchmark/<tag>/benchmark_summary.csv \
  --output-prefix docs/benchmark/<tag>/benchmark_gate
```

### 一键门禁

```bash
./tools/run_cache_gate_local.sh --run-tag <tag>
```


## 测试结果（2026-04-14 最终运行）

### 正确性：全部通过 

| 范围 | 结果 |
|---|---|
| CTest | **20/20** PASS |
| rv32ui p1 | **42/42** PASS |
| rv32ui p10 | **42/42** PASS |
| rv32ui no-cache | **42/42** PASS |
| benchmark gate | PASS (issues=0) |
| benchmark cache gate | PASS (issues=0) |
| web smoke | PASS |

### Benchmark 性能

| 用例 | Cache (p10) | No-Cache | 加速比 |
|---|---:|---:|---:|
| hello | 161 | 1,518 | **9.43×** |
| matmul (16×16) | 277,966 | 3,151,861 | **11.34×** |
| quicksort_stress | 1,972,901 | 25,074,548 | **12.71×** |

- matmul D-Cache 命中率 **99.50%**，quicksort **99.86%**
- 平均加速比 **11.16×**

### rv32ui Cache 策略矩阵

| 策略 | 通过 | 平均周期 | I-Hit | D-Hit | 加速比 |
|---|---:|---:|---:|---:|---:|
| **wb_wa** | 42/42 | 696.88 | 93.50% | 19.41% | **6.47×** |
| wb_nowa | 42/42 | 698.88 | 93.50% | 18.34% | 6.46× |
| wt_wa | 42/42 | 698.88 | 93.50% | 18.34% | 6.46× |
| wt_nowa | 42/42 | 698.88 | 93.50% | 18.34% | 6.46× |
| nocache | 42/42 | 4,633.83 | — | — | 1.00× |

- **结论**: wb_wa（写回 + 写分配）在当前样本下综合最优，已纳入性能门禁基线。


## Web 可视化

项目附带基于 HTML + D3.js 的实时可视化仪表盘：

- 五级流水线气泡图（IF/ID/EX/MEM/WB）
- 指令甘特图（最近 32 cycle）
- 性能折线图（IPC、D-Hit、Cache Stall 占比）
- 寄存器窗口（x0–x31，高亮当周期写回）
- Memory Inspector（按地址回放）
- Virtual Console（UART 输出字符提取）

### 启动

```bash
./tools/run_trace_demo.sh benchmarks/hello.elf 200
# 浏览器访问脚本打印的 URL，例如 http://localhost:8080
```


## 技术栈

- **语言**: C++17
- **构建**: CMake 3.10+
- **测试**: CTest + rv32ui (RISC-V 官方测试套件)
- **工具链**: RISC-V GNU Toolchain (riscv64-unknown-elf-gcc)
- **脚本**: Bash + Python 3
- **可视化**: HTML5 + D3.js + SSE
