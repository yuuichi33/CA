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

### 2.1 CPU 与 ISA

- 五级流水：IF/ID/EX/MEM/WB。
- 支持 load-use 冒险暂停、前递、分支冲刷。
- 支持 RV32I 主体指令和系统指令（ECALL/MRET/SRET/CSR/FENCE/SFENCE.VMA）。
- 指令识别数量：49（不含占位标签 UNKNOWN/SYSTEM）。

### 2.2 内存、Cache、MMU

- 物理内存读写（8/16/32 位）+ 越界访问检测。
- I-cache / D-cache 分离；支持命中率、miss、evict、writeback 统计。
- 可配置 cache 参数：容量、line size、组相联、写回/写直达、写分配。
- Sv32 页表走访、ASID、TLB、A/D 位更新、SFENCE.VMA 刷新。

### 2.3 异常、中断、外设

- trap 入口、返回、委派：M/S/U 基础路径已经实现。
- Timer 中断：周期触发或 mtimecmp 触发。
- UART MMIO：发送、接收 FIFO、IRQ 触发。
- TOHOST 机制：支持 riscv-tests 风格退出码回传。

### 2.4 工具链与可视化

- ELF 加载（含 ET_EXEC/ET_DYN 基础处理与部分重定位）。
- Trace JSON 输出 + SSE + Web 仪表盘。
- 一键跑全量脚本、CSV 与图表报告生成脚本已就绪。

## 3. 当前验证结果（用于进度汇报的数据）

- CTest：19/19 通过。
- rv32ui：
  - p1：42/42 通过。
  - p10：42/42 通过。
  - no-cache：42/42 通过。
- benchmark：
  - matmul cache vs no-cache：277966 vs 3151861 cycles，约 11.34x。
  - quicksort cache vs no-cache：约 12.71x。


## 4. 还需要补哪些功能

### 4.1 为课程主线建议补齐（优先级高）

1. 中断控制器抽象再前进一步
   - 当前是 timer/uart 直接驱动 CSR pending 位。
   - 建议增加一个轻量中断汇聚层，便于后续扩展更多外设。
2. 异常/中断上下文文档化
   - 代码已实现 trap 入口与 MRET/SRET，建议补一页“上下文保存恢复流程图”。
3. MMU 基础增强（可选但加分）
   - 目前是基础 Sv32；可继续补超级页、更多权限边界测试。

### 4.2 若要跑最小 miniOS（可选）

最低可行目标（MVP）建议：

1. 启动与串口
   - 进入 C 内核入口，UART 打印 boot banner。
2. Trap 框架
   - ECALL + timer 中断统一入口，能打印 trap 原因。
3. 时钟滴答
   - 定时器周期中断，驱动全局 tick 计数。
4. 最小调度演示
   - 两个任务轮转（哪怕只是协作式），证明“OS 在你 CPU 上跑起来”。

说明：以当前硬件抽象，做一个教学版 miniOS 是可行的；直接跑 Linux 仍需更多特性（如更完整特权架构、原子扩展、启动协议与设备模型）。

## 5. 建议的后续计划（可直接汇报）

### 稳定化与口径统一

- 固化一键复现实验脚本（构建、测试、报告）。

### 中断与内核接口增强

- 增加轻量中断汇聚层。
- 补 trap/返回流程图和关键代码注释。

### miniOS MVP（可选加分）

- 完成 UART 控制台 + timer tick + 简单任务切换演示。
- 输出 miniOS 演示日志与截图，形成闭环材料。

## 6. 环境适配说明（汇报可选）

- 当前工程是 CMake + C++17 + Bash 脚本，已适配 Linux 命令链。
- 已知注意点：
   - 进程与端口管理建议优先用脚本，避免手动多开导致端口冲突。
   - 大规模测试时建议使用 --quiet，减少终端 I/O 对性能统计的扰动。
   - 报告与图表生成均为 Python 标准库实现，不依赖额外图形后端。

## 7. 汇报时可直接使用的总结话术

本项目已完成一台可独立运行的 RV32I 教学模拟器 myCPU。核心上实现了五级流水、I/D Cache、Sv32 基础 MMU、异常与中断机制，以及 UART/Timer 外设接口；在验证上通过 19 项 CTest 与 42 项 rv32ui 全量测试，并在矩阵乘 benchmark 上获得约 11.34 倍加速（含 p1 CSV 去重后的统一口径）。下一阶段将重点做数据质量防回归与中断抽象增强，可选完成一个最小 miniOS 运行演示，以形成“自研 CPU 跑自研 OS”的课程闭环。
