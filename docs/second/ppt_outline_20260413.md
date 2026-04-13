# myCPU 进度汇报 PPT（Cache 主线版，建议 12 页）

使用说明：

- 每页都包含三部分：PPT 上展示的文字、PPT 上展示的图片、你现场口播。
- 建议总时长 10-12 分钟，其中 Cache 相关页建议占 6-7 分钟。

## 第 1 页：封面

### PPT 上展示的文字

- 标题：myCPU：从 0 实现可运行程序的 RISC-V 指令集模拟器
- 副标题：以 Cache 为主线的阶段成果汇报
- 信息：课程名、姓名、日期

### PPT 上展示的图片

- 一张简洁硬件背景图（弱透明，不影响阅读）。

### 你现场说什么

这次汇报我会围绕一个核心问题展开：我实现的 Cache 到底带来了什么价值。除了介绍整体架构，我会重点展示 Cache 的实现、验证和性能收益，最后给出下一阶段计划。

## 第 2 页：项目目标与本次汇报重点

### PPT 上展示的文字

- 项目主目标：交付可运行的 myCPU 模拟器
- 当前状态：主目标已完成
- 本次汇报重点：
  - Cache 设计是否完整
  - Cache 正确性是否可信
  - Cache 性能收益是否可量化

### PPT 上展示的图片

- 三段式图：功能实现 -> 正确性验证 -> 性能量化。

### 你现场说什么

项目目标不是做一个只能跑样例的解释器，而是做一个可验证、可分析的教学级模拟器。今天我把重点放在 Cache，因为这是这阶段最关键的性能特性，也最能体现工程完整性。

## 第 3 页：总体架构（给 Cache 定位）

### PPT 上展示的文字

- CPU：Decoder + 5 级 Pipeline + CSR
- Memory 子系统：MMU + I-cache + D-cache + 物理内存
- 外设：UART / Timer / TOHOST
- 工具链：ELF Loader + Trace + Web + 自动测试

### PPT 上展示的图片

- 一张模块图：重点高亮 IF 到 I-cache、MEM 到 D-cache 两条路径。

### 你现场说什么

这页主要交代 Cache 在系统里的位置。I-cache 挂在取指路径，D-cache 挂在访存路径，二者都直接影响流水线停顿。也就是说，Cache 不是独立模块，而是执行主路径上的关键性能部件。

## 第 4 页：流水线与 Cache 的协同机制

### PPT 上展示的文字

- 五级流水：IF/ID/EX/MEM/WB
- Cache miss 行为：转化为流水线 stall
- 冒险统计：stall / cache_stall / hazard_stall 分离统计
- 作用：可区分“缓存瓶颈”和“数据相关瓶颈”

### PPT 上展示的图片

- 一张时序图：命中时正常推进、miss 时插入停顿周期。

### 你现场说什么

实现上我把 cache miss 明确映射成流水线停顿，这样性能分析就能分层。比如 stall 变多，到底是 cache miss 造成，还是 load-use 冒险造成，可以直接从统计项里看出来。

## 第 5 页：已实现指令清单（类别 + 条数 + 列表）

### PPT 上展示的文字

- 总有效指令数：49（排除占位项 UNKNOWN、SYSTEM）
- 分类统计：
  - R型算术 10
  - I型算术 9
  - Load 5
  - Store 3
  - Branch 6
  - U型 2
  - JAL 1
  - JALR 1
  - CSR/特权 10
  - Fence 2

- 详细列表：
  - R型算术：ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND
  - I型算术：ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI, ANDI
  - Load：LB, LH, LW, LBU, LHU
  - Store：SB, SH, SW
  - Branch：BEQ, BNE, BLT, BGE, BLTU, BGEU
  - U型：LUI, AUIPC
  - 跳转：JAL, JALR
  - CSR/特权：ECALL, MRET, SRET, SFENCE.VMA, CSRRW, CSRRS, CSRRC, CSRRWI, CSRRSI, CSRRCI
  - Fence：FENCE, FENCE.I

### PPT 上展示的图片

- 一张类别-条数柱状图（10类）。

### 你现场说什么

课程要求 30 到 40 条基础指令，我当前实现了 49 条有效指令，覆盖算术、访存、控制流和系统控制。这个覆盖度是 Cache 测试可信的前提，因为只有指令类型够完整，性能结论才有代表性。

## 第 6 页：Cache 设计细节（实现层）

### PPT 上展示的文字

- 架构：I-cache / D-cache 分离
- 参数可配置：cache_size, line_size, associativity, miss_latency
- 写策略可配置：write-back / write-through
- 写分配可配置：write-allocate / no-write-allocate
- 统计项：accesses, hits, misses, evictions, writebacks

### PPT 上展示的图片

- 一张参数表截图或手工表格。
- 一张 cache line + set/way 示意图。

### 你现场说什么

我把 Cache 设计成可配置模型，不是写死参数。这样可以做课程里的对比实验，比如 penalty 从 1 改到 10，或者切换写策略，看性能和行为如何变化。统计项也完整保留，便于后续分析。

## 第 7 页：Cache 正确性证据

### PPT 上展示的文字

- CTest：19/19 通过
- rv32ui：
  - p1：42/42
  - p10：42/42
  - no-cache：42/42
- 口径说明：三组均为 42 项唯一测试

### PPT 上展示的图片

- 一张结果总表（ctest + rv32ui 三配置）。
- 可选：终端输出截图（100% passed）。

### 你现场说什么

先谈性能前，我先证明正确性。Cache 开启和关闭两种路径都通过了完整 rv32ui 测试，说明不是“只在快路径上正确”。这样后面的速度提升才是可信收益，不是错误导致的假加速。

## 第 8 页：Cache 性能证据（核心数字）

### PPT 上展示的文字

- rv32ui p10 平均 speedup：6.47x
- p10/p1 平均 cycle 比：1.54x
- I-hit 与 speedup 相关系数：0.703
- D-hit 与 speedup 相关系数：0.530

### PPT 上展示的图片

- docs/figures/full_run_20260409_speedup_bar.png
- docs/figures/full_run_20260409_hitrate_scatter.png

### 你现场说什么

这个结果说明两点。第一，Cache 对整体性能有稳定收益，不是个别用例偶然快。第二，I-cache 对性能更敏感，因为取指发生在每条指令上，这也解释了为什么提升取指命中率的收益更直接。

## 第 9 页：Benchmark 场景下的 Cache 收益

### PPT 上展示的文字

- matmul：3151861 -> 277966 cycles，11.34x
- quicksort：25074548 -> 1972901 cycles，12.71x
- 结论：真实负载下 Cache 收益达到 11x-12x

### PPT 上展示的图片

- docs/figures/full_run_20260409_benchmark_cycles_log.png

### 你现场说什么

在 benchmark 场景里，Cache 收益更明显，尤其是局部性较强的计算和重访存负载。这个量级的提升说明当前 Cache 设计不仅理论可行，而且在实际程序上有效。

## 第 10 页：可观测性与工程化能力

### PPT 上展示的文字

- 一键流程：build -> test -> csv -> report -> figures
- 可观测：Trace JSON + SSE + Web Dashboard
- 可复现：同脚本、同口径、可重复结果

### PPT 上展示的图片

- 一张自动化流程图。
- 一张 Web 仪表盘截图。

### 你现场说什么

这页强调“工程闭环”。我不仅实现了 Cache，还实现了可以持续验证 Cache 的工具链。这样每次改动都能快速看见正确性和性能有没有回退。

## 第 11 页：差距与风

### PPT 上展示的文字

- 当前差距：
  - miss 类型拆分还不够细（冷启动/冲突/容量）
  - 中断控制器仍是简化直连
  - MMU 还有增强空间
- 风险控制：
  - 固化 Cache 回归基线
  - 增加关键指标门禁
  - 保持统一数据口径

### PPT 上展示的图片

- 一张风险矩阵图（影响度 x 发生概率）。

### 你现场说什么

我认为最大的风险不是“现在跑不通”，而是后续迭代时性能悄悄回退。所以我下一步重点会放在 Cache 回归自动化和指标门禁上，把当前收益稳定住。

## 第 12 页：后续计划与收尾

### PPT 上展示的文字

- 里程碑A：Cache 回归固化（1 周）
- 里程碑B：Cache 可解释性增强（1-2 周）
- 里程碑C：中断抽象 + miniOS MVP（可选加分）
- 最终目标：自己写 CPU -> 自己写 OS -> 自己跑程序

### PPT 上展示的图片

- 一张三阶段时间轴图（Now -> Next -> Future）。

### 你现场说什么

总结一下：本阶段我已经把 Cache 做到可实现、可验证、可量化。下一阶段会先把 Cache 的回归体系做扎实，再向系统能力推进，最终形成 CPU 到 OS 的闭环展示。谢谢。
