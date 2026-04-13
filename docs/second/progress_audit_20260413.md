# myCPU 项目进度审计（2026-04-13）

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

## 3. 当前验证结果（Cache 视角）

### 3.1 正确性

- CTest：19/19 通过。
- rv32ui：p1/p10/no-cache 均 42/42 通过。

### 3.2 Cache 性能关键数字

- rv32ui p10 平均 speedup：6.46x。
- p10/p1 平均 cycle 比：1.54x。
- D-hit 与 speedup 相关系数：0.475。
- I-hit 与 speedup 相关系数：0.711。

### 3.3 Benchmark（cache on vs no-cache）

| 用例 | cache on cycles | no-cache cycles | 加速比 |
|---|---:|---:|---:|
| matmul | 277966 | 3151861 | 11.34x |
| quicksort | 1972901 | 25074548 | 12.71x |

### 3.4 Cache 矩阵回归与门禁（20260413）

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

### 3.5 可汇报结论

- Cache 在教学 workload 上带来稳定且显著的收益（6x-12x 量级）。
- 当前架构中 I-cache 对整体性能更敏感（相关系数更高）。
- 正确性与性能已经形成闭环：全量通过 + 可量化收益。

## 4. 还需要补哪些功能（按优先级）

### 4.1 Cache 主线优先项

1. Cache 回归基线自动化（已完成）
   - 已新增 `tools/run_cache_matrix.sh`，可批量执行 WB/WT x WA/no-WA + no-cache 组合。
   - 产物目录固定为 `docs/cache_matrix/<run-tag>/`，包含每策略原始 CSV 与矩阵汇总。
2. Cache 指标门禁（已完成）
   - 已新增 `tools/check_cache_gate.py`，读取 `policy_summary.csv` 输出 PASS/WARN/FAIL。
   - 同步产出 `gate_checks.csv`、`gate_result.json`、`gate_report.md`，可直接接入 CI。
3. Cache 可解释性增强（已完成）
   - 已在 cache 核心实现 miss 分解：cold/conflict/capacity。
   - 指标已打通到 Pipeline 统计、Trace JSON、`test_all.sh` CSV 与最终性能行。
4. Cache 专项验证（已完成）
   - 新增 `tests/test_cache_miss_types.cpp` 并接入 CTest，覆盖冲突/容量 miss 分类回归。

### 4.2 门禁执行链路（已落地）

1. 本地阻断入口（已完成）
   - 新增 `tools/run_cache_gate_local.sh`，串联 build、ctest、matrix、gate 全链路。
   - 默认策略：PASS=0，WARN/FAIL 非零阻断。
2. CI 就绪模板（已完成）
   - 新增 `.github/workflows/cache-gate.yml`（当前手动触发）。
   - 后续可直接打开 PR/push 触发实现自动阻断。

### 4.3 文档治理与归档（已完成）

1. 历史产物归档
   - 已将 20260409 的 summary 与 figure 迁移到 `archive/docs-history/`。
2. 索引同步
   - 已更新 `docs/DOCS_CATALOG.md` 与 `archive/MANIFEST.md`，标注 current/historical。
3. second 目录完善
   - 新增 cache gate 使用说明、验收检测清单、可视化演示指南与检测报告。

### 4.4 体系结构增强项

1. 中断控制器抽象
   - 从 timer/uart 直连 CSR，演进到统一中断汇聚层。
2. 异常/中断文档化
   - 补齐 trap 上下文保存恢复流程图。
3. MMU 增强
   - 在基础 Sv32 上补充更多权限边界与页表场景测试。

### 4.5 miniOS MVP（可选）

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

## 7. 汇报时可直接使用的总结话术

本项目已完成一台可独立运行的 RV32I 教学模拟器 myCPU。实现上覆盖 49 条有效指令，完成五级流水、I/D Cache、Sv32 基础 MMU、异常与中断、UART/Timer 外设。验证上通过 20 项 CTest 与 42 项 rv32ui 三配置全量测试。性能上，cache 在 rv32ui 提供 6.46x 平均加速，在 matmul 与 quicksort 分别达到 11.34x 和 12.71x。下一阶段将以 Cache 回归固化与可解释性增强为主线，并推进中断抽象与 miniOS MVP 演示，形成从 CPU 到 OS 的课程闭环。
