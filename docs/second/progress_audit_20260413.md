# myCPU 项目进度审计（2026-04-13，更新于 2026-04-14）

## 1. 对照课程要求的完成度

| 课程要求 | 当前状态 | 结论 | 证据位置 |
|---|---|---|---|
| 产出可独立运行的 myCPU 模拟器 | 已完成 | 可执行文件 mycpu 已支持加载 ELF 并运行 | src/main.cpp, CMakeLists.txt |
| 五级流水线 | 已完成 | IF/ID/EX/MEM/WB 全链路实现，含前递与冒险处理 | src/cpu/pipeline.h, src/cpu/pipeline.cpp |
| I/D Cache | 已完成 | I-cache 与 D-cache 分离，可配置组相联、写策略、miss penalty | src/cache/simple_cache.h, src/cache/simple_cache.cpp, src/cpu/pipeline.cpp |
| 30-40 条基础指令 | 已完成（超出） | 实现 49 条可识别指令（不含 UNKNOWN/SYSTEM 占位） | src/isa/decoder.cpp, src/cpu/pipeline.cpp |
| 内存与地址空间模拟 | 已完成（基础） | 物理内存 + MMIO 映射 + 越界检查 + 异常上报 | src/mem/memory.cpp |
| MMU/分页机制（基础版） | 已完成（基础） | Sv32 两级页表 + TLB + SFENCE.VMA | src/mmu/mmu.cpp, src/cpu/pipeline.cpp |
| 中断与异常模块 | 已完成（课程可用） | ECALL/MRET/SRET、trap delegation、timer/uart interrupt | src/cpu/csr.cpp, src/cpu/pipeline.cpp |
| 外设接口（UART、定时器） | 已完成 | UART MMIO、Timer MMIO、TOHOST semihosting 已接入 | src/periph/uart_mmio.cpp, src/periph/timer_mmio.cpp, src/periph/tohost_mmio.cpp |
| 统一硬件接口规范 | 已完成（基础） | 统一 Device 接口与 map_device 机制 | src/mem/memory.h, src/mem/memory.cpp |
| 完整技术报告 | 已完成 | 已有全量测试与性能报告产物 | docs/FULL_TEST_PERFORMANCE_REPORT.md, docs/BENCHMARK_REPORT.md |
| 在模拟器上运行 miniOS/Linux | 未完成（下阶段可选） | 当前具备最小内核实验条件，但尚无完整 OS 镜像 | 现有仓库暂无 OS 目录/镜像 |

结论：本阶段主目标（虚拟机 myCPU）已经达成，且在流水线、Cache、MMU、中断、外设上都达到“可演示、可测试、可量化”的状态。

## 2. 当前已经实现了什么

### 2.1 CPU 微结构

- 五级流水：IF/ID/EX/MEM/WB。
- 数据相关：实现 forwarding 和 load-use 停顿。
- 控制相关：实现分支冲刷、异常/中断冲刷流水。
- 统计能力：提供 cycles、instrs、stall、cache_stall、hazard_stall。

### 2.2 ISA 覆盖清单（按类别）

当前有效指令总数为 49（排除占位项 UNKNOWN、SYSTEM）。

| 类别 | 条数 | 指令 |
|---|---:|---|
| R型算术 | 10 | ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND |
| I型算术 | 9 | ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI |
| Load | 5 | LB, LH, LW, LBU, LHU |
| Store | 3 | SB, SH, SW |
| Branch | 6 | BEQ, BNE, BLT, BGE, BLTU, BGEU |
| U型 | 2 | LUI, AUIPC |
| JAL | 1 | JAL |
| JALR | 1 | JALR |
| CSR/特权 | 10 | ECALL, MRET, SRET, SFENCE.VMA, CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI |
| Fence | 2 | FENCE, FENCE.I |

说明：课程要求 30-40 条基础指令，当前实现为 49 条有效指令，已超出最低目标并覆盖系统级运行所需指令。

### 2.3 Cache 实现细节（本次汇报重点）

1. 架构与接口
   - I-cache 与 D-cache 分离。
   - 统一由 Pipeline 在 IF/MEM 阶段调用，cache miss 会转化为流水线 stall。
2. 可配置能力
   - 支持容量、line size、组相联度、miss penalty。
   - 支持写策略：write-back / write-through。
   - 支持写分配策略：write-allocate / no-write-allocate。
3. 可观测指标
   - accesses、hits、misses、evictions、writebacks。
   - 可输出 I-hit/D-hit 与 stall 分解（cache_stall/hazard_stall）。
4. 与正确性的协同
   - 对特定直接访存路径做 flush/invalidate 协同处理，避免 cache 与直访路径不一致。

### 2.4 内存与 MMU

- 物理内存读写（8/16/32 位）+ 越界异常检测。
- MMIO 设备映射机制统一在 Memory 层管理。
- Sv32 两级页表、TLB、ASID、A/D 位更新、SFENCE.VMA。

### 2.5 异常、中断、外设

- trap 入口与返回：ECALL/MRET/SRET。
- 中断源：Timer 与 UART。
- 外设：UART MMIO、Timer MMIO、TOHOST semihosting。

### 2.6 工具链与可视化

- ELF 加载（含 ET_EXEC/ET_DYN 基础处理与部分重定位）。
- Trace JSON + SSE + Web 可视化。
- 一键脚本与自动报告，支持复现实验与答辩展示。

### 2.7 Benchmark 自动化与门禁链路（新增细化）

1. benchmark 三配置结构化跑数
   - `tools/run_benchmark_profiles.sh` 已稳定产出 `benchmark_p1/p10/nocache.csv`、`benchmark_detail.csv`、`benchmark_summary.csv`。
2. benchmark cache matrix 跑数
   - `tools/run_benchmark_cache_matrix.sh` 已支持 5 策略（`wb_wa/wb_nowa/wt_wa/wt_nowa/nocache`）x 3 benchmark 批量运行。
   - 已产出策略级汇总 `benchmark_policy_summary.csv` 与样例级明细 `benchmark_matrix_detail.csv`。
3. benchmark cache gate
   - `tools/check_benchmark_cache_gate.py` 已支持 PASS/WARN/FAIL 判定与阈值参数化。
   - 已产出 `benchmark_cache_gate_checks.csv`、`benchmark_cache_gate_result.json`、`benchmark_cache_gate_report.md`。
4. 本地阻断链路整合
   - `tools/run_cache_gate_local.sh` 已整合 rv32ui cache gate + benchmark gate + benchmark cache gate（三级门禁）。
5. 报告链路整合
   - `tools/gen_full_test_report.py` 已接入 benchmark cache matrix 与 web smoke 采样状态，报告中可直接展示策略对比与门禁结果。

## 3. 当前验证结果（Cache 视角）

### 3.1 正确性

- CTest：20/20 通过。
- rv32ui：p1/p10/no-cache 均 42/42 通过。

### 3.2 Cache 性能关键数字

- rv32ui p10 平均 speedup：6.46x。
- p10/p1 平均 cycle 比：1.54x。
- D-hit 与 speedup 相关系数：0.475。
- I-hit 与 speedup 相关系数：0.711。

### 3.3 Benchmark（结构化三配置）

| 用例 | p10 cycles | no-cache cycles | 加速比 |
|---|---:|---:|---:|
| hello | 161 | 1518 | 9.43x |
| matmul | 277966 | 3151861 | 11.34x |
| quicksort_stress | 1972901 | 25074548 | 12.71x |

- 数据来源：`docs/benchmark/20260414/benchmark_summary.csv`
- benchmark gate：PASS（`docs/benchmark/20260414/benchmark_gate_result.json`）

### 3.4 Benchmark Cache Matrix（20260413）

| 策略 | pass/benchmarks | avg cycles | avg I-hit | avg D-hit | vs no-cache speedup |
|---|---:|---:|---:|---:|---:|
| wb_wa | 3/3 | 750342.67 | 99.71% | 98.20% | 11.1590x |
| wb_nowa | 3/3 | 751638.67 | 99.71% | 93.95% | 11.1440x |
| wt_wa | 3/3 | 751638.67 | 99.71% | 93.95% | 11.1440x |
| wt_nowa | 3/3 | 751638.67 | 99.71% | 93.95% | 11.1440x |
| nocache | 3/3 | 9409309.00 | 0.00% | 0.00% | 1.0000x |

- benchmark cache gate：PASS（baseline=`wb_wa`，issues=0）。
- 关键证据：
   - `docs/cache_matrix/20260413/benchmark_policy_summary.csv`
   - `docs/cache_matrix/20260413/benchmark_matrix_detail.csv`
   - `docs/cache_matrix/20260413/benchmark_cache_gate_result.json`
   - `docs/cache_matrix/20260413/benchmark_cache_gate_report.md`
- 结果解读：
   - `wb_wa` 在当前 benchmark 集合下保持最优，较 `wb_nowa/wt_*` 的 D-hit 高约 4.25 个百分点。
   - 相比 no-cache，cache 策略在 benchmark 上保持约 11x 量级平均加速。

### 3.5 Cache 矩阵回归与门禁（rv32ui，20260413）

| 策略 | 通过率 | avg cycles | avg I-hit | avg D-hit | vs no-cache speedup |
|---|---:|---:|---:|---:|---:|
| wb_wa | 42/42 | 696.88 | 93.50% | 19.41% | 6.4705x |
| wb_nowa | 42/42 | 698.88 | 93.50% | 18.34% | 6.4553x |
| wt_wa | 42/42 | 698.88 | 93.50% | 18.34% | 6.4553x |
| wt_nowa | 42/42 | 698.88 | 93.50% | 18.34% | 6.4553x |
| nocache | 42/42 | 4633.83 | 0.00% | 0.00% | 1.0000x |

- 门禁脚本输出：PASS（baseline=wb_wa，issues=0）。
- miss 分解（p10/wt_nowa）：
   - I-miss 占比：cold 87.4% / conflict 12.6% / capacity 0.0%
   - D-miss 占比：cold 14.4% / conflict 85.6% / capacity 0.0%

### 3.6 Web smoke 重采样

- 采样脚本：`tools/sample_web_smoke.sh`
- 最新结果：PASS
   - `tmp/full_run_20260413/web_health.json`
   - `tmp/full_run_20260413/web_index_head.txt`
   - `tmp/full_run_20260413/web_smoke_status.json`
- 说明：采样失败按 WARN 记录，不阻断报告生成。

### 3.7 可汇报结论

- Cache 在教学 workload 上带来稳定且显著的收益（6x-12x 量级）。
- 当前架构中 I-cache 对整体性能更敏感（相关系数更高）。
- benchmark cache matrix 显示 `wb_wa` 在当前样本下综合最优，策略差异已可被门禁脚本量化捕获。
- 正确性与性能已经形成闭环：全量通过 + 可量化收益。

## 4. 后续待办（仅保留未完成项）

### 4.1 CI 自动阻断启用

1. 将 `.github/workflows/cache-gate.yml` 从手动触发切换为 PR/push 自动触发。
2. 固化三类门禁阈值分层（本地宽松、预提交中等、CI严格），减少人工判读。

### 4.2 体系结构增强项

1. 中断控制器抽象
   - 从 timer/uart 直连 CSR，演进为统一中断汇聚层，降低外设耦合。
2. 异常/中断流程文档化
   - 增补 trap 上下文保存/恢复流程图与典型异常路径示例。
3. MMU 测试增强
   - 在 Sv32 基础上补充权限边界、页表异常和 TLB 一致性专项回归用例。

### 4.3 Benchmark 体系增强

1. 扩展 benchmark 样本
   - 增加访存模式差异更大的 workload，用于放大策略差异。
2. 建立跨 run-tag 趋势对比
   - 在报告中增加 benchmark cache matrix 历史趋势（cycles/hit/speedup）变化视图。

### 4.4 miniOS MVP（可选）

1. 启动与串口输出。
2. trap 框架（ECALL + timer interrupt）。
3. tick 驱动与最小调度演示。

说明：当前硬件抽象已具备 miniOS 入口条件；直接跑 Linux 仍需要更多指令扩展与平台能力。

## 5. 环境适配说明（汇报可选）

- 当前工程是 CMake + C++17 + Bash，Linux/WSL 可运行。
- 建议：
  - 优先脚本化启动，减少端口与进程冲突。
  - 大规模性能测试使用 quiet 模式，降低终端 I/O 噪声。
  - 报告图表生成依赖 Python 标准库，环境依赖较轻。

## 6. 汇报时可直接使用的总结话术

本项目已完成一台可独立运行的 RV32I 教学模拟器 myCPU。实现上覆盖 49 条有效指令，完成五级流水、I/D Cache、Sv32 基础 MMU、异常与中断、UART/Timer 外设。验证上通过 20 项 CTest 与 42 项 rv32ui 三配置全量测试。性能上，cache 在 rv32ui 提供 6.46x 平均加速，在 matmul 与 quicksort 分别达到 11.34x 和 12.71x；benchmark cache matrix 显示 `wb_wa` 在当前样本下综合最优并已纳入门禁。下一阶段将推进 CI 自动阻断启用、中断抽象、MMU 边界测试与 miniOS MVP，形成从 CPU 到 OS 的课程闭环。
