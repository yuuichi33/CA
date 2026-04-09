# myCPU

myCPU 是一个面向教学与微架构实验的 RISC-V 仿真器，采用 C++17 实现，核心是五级流水线 RV32I CPU，支持特权态、MMU（Sv32）、I/D Cache、UART/Timer/MMIO 与 ELF 加载。

## 项目简介

### 架构特点
- 五级流水：IF / ID / EX / MEM / WB。
- 指令集基线：RV32I，并扩展实现了 CSR、特权态系统指令与内存屏障指令。
- 特权态支持：M/S/U 三态，含 trap 入口/返回与中断委派路径。
- 内存系统：Sv32 两级页表 + 4 项全相联 TLB（带 ASID）。
- 缓存系统：独立 I-Cache / D-Cache，默认 4-way、写回 + 写分配，支持 miss penalty 建模。

## 核心硬核指标

### 指令与执行能力
- 已解码并可执行的具体指令：48 条（不含 `UNKNOWN` / `SYSTEM` 占位）。
- 指令类别覆盖：
  - 算术逻辑：ADD/SUB/SLL/SRL/SRA/SLT/SLTU/XOR/OR/AND。
  - 立即数：ADDI/SLLI/SRLI/SRAI/SLTI/SLTIU/XORI/ORI/ANDI。
  - 访存：LB/LBU/LH/LHU/LW/SB/SH/SW。
  - 控制流：BEQ/BNE/BLT/BGE/BLTU/BGEU/JAL/JALR。
  - 系统与特权：ECALL/MRET/SRET/CSRRW/CSRRS/CSRRC/CSRRWI/CSRRSI/CSRRCI/SFENCE.VMA/FENCE/FENCE.I。

### MMU（Sv32）能力
- 两级页表遍历（Sv32 模式，`satp` 管理根页表与 ASID）。
- 页级权限检查（R/W/X/U）与异常码返回（取指/读/写页故障）。
- A/D 位自动更新。
- 4 项全相联 TLB，按 ASID 命中，支持 round-robin 替换。
- `SFENCE.VMA` 语义支持（全刷、按 ASID、按 VPN、按 VPN+ASID）。
- 当前限制：superpage 未实现（检测到上级叶子项时按 fault 处理）。

### Cache 实现与策略
- I/D 分离缓存。
- 运行配置（当前 CLI 默认）：16KB、64B line、4-way、write-back、write-allocate。
- 统计能力：I/D 访问次数与命中次数，输出命中率。
- 屏障一致性：`FENCE.I` 触发 D-Cache flush + I-Cache invalidate。

## 验证与跑测结果

### 官方 ISA 测试
- 已通过 `riscv-tests/isa/rv32ui-p-*` 全量 42/42。
- 脚本入口：[test_all.sh](test_all.sh)。

示例命令：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
bash test_all.sh --csv docs/rv32ui_perf.csv --cache-penalty 10
```

### 结果产物
- cache 基线：[docs/rv32ui_perf.csv](docs/rv32ui_perf.csv)
- no-cache 基线：[docs/rv32ui_perf_nocache.csv](docs/rv32ui_perf_nocache.csv)
- 加速比报告：[docs/SPEEDUP_REPORT.md](docs/SPEEDUP_REPORT.md)
- matmul 基准报告：[docs/BENCHMARK_REPORT.md](docs/BENCHMARK_REPORT.md)

## 性能亮点

### rv32ui 全量对比（cache vs no-cache，penalty=10）
- 对比样本：42 项。
- 正确性前提：cache/no-cache 双端均为 42/42 PASS。
- 平均加速比（NoCache/Cache）：6.47x。

### Matmul 演示（16x16）
- cache：`Cycles=277966, Instrs=230400, I-Hit=99.98%, D-Hit=99.50%`
- no-cache：`Cycles=3151861, Instrs=230400, I-Hit=0.00%, D-Hit=0.00%`
- 加速比（NoCache/Cache）：11.34x

## 快速上手

### 1) 编译

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 2) 一键运行 rv32ui 全量

```bash
bash test_all.sh --csv docs/rv32ui_perf.csv --cache-penalty 10
```

### 3) 运行 Matmul 性能演示

```bash
# cache on
./build/mycpu --quiet --load benchmarks/matmul.elf --cache-penalty 10

# cache off
./build/mycpu --quiet --load benchmarks/matmul.elf --no-cache --cache-penalty 10
```

### 4) 生成 JSONL Trace（步骤3）

```bash
# 输出到文件
./build/mycpu --load benchmarks/hello.elf --trace-json tmp/hello_trace.jsonl --max-cycles 500

# 输出到 stdout（可被 trace server 管道消费）
./build/mycpu --load benchmarks/hello.elf --trace-json stdout --max-cycles 500

# 断点示例（PC 命中即停止）
./build/mycpu --load benchmarks/hello.elf --trace-json tmp/hello_trace.jsonl --breakpoint 0x100 --max-cycles 1000
```

### 5) Trace 中间件与浏览器监视（步骤4, WSL1）

当前仓库提供零依赖 Python 版本 trace server：

```bash
chmod +x tools/run_trace_demo.sh
./tools/run_trace_demo.sh
```

浏览器打开：`http://localhost:8080`

健康检查：

```bash
curl -s http://localhost:8080/health
```

### 6) D3 可视化面板（步骤5）

启动步骤4服务后，打开浏览器：`http://localhost:8080`。

页面已实现：
- 播放/暂停/单步/速率控制/按 cycle 跳转。
- 五级流水线视图（IF/ID/EX/MEM/WB）。
- 最近 32 cycle 的指令时间轴。
- 寄存器窗口 x0..x31（按 `regs_changed` 高亮）。
- I/D Cache 命中率、cache 事件与当周期访存事件。
- Memory Inspector（按地址窗口回放，基于 `SB/SH/SW` store 事件重建已知字节）。

详情见：[web/README.md](web/README.md)

## 项目结构（File Tree）

说明：以下是当前工作区的结构化树；`build/` 与 `riscv-tests/` 内部内容非常大，树中只展开关键层级。

```text
camycpu/
├── .clang-format
├── .gitignore
├── CMakeLists.txt
├── README.md
├── tmp_min_elf.bin
├── test_all.sh
├── Testing/
│   └── Temporary/
│       ├── CTestCostData.txt
│       └── LastTest.log
├── benchmarks/
│   ├── hello.c
│   ├── hello.elf
│   ├── linker.ld
│   ├── matmul.c
│   ├── matmul.elf
│   ├── matmul.map
│   └── start.S
├── docs/
│   ├── BENCHMARK_REPORT.md
│   ├── ISA.md
│   ├── SPEEDUP_REPORT.md
│   ├── rv32ui_perf.csv
│   └── rv32ui_perf_nocache.csv
├── riscv-tests/                     (Git submodule, 上游官方仓库)
│   ├── isa/
│   │   ├── rv32ui/
│   │   └── rv32ui-p-* (42个预编译测试 ELF)
│   ├── benchmarks/
│   ├── env/
│   ├── debug/
│   └── mt/
├── src/
│   ├── main.cpp
│   ├── trace/
│   │   ├── trace.h
│   │   └── trace.cpp
│   ├── cache/
│   │   ├── cache_config.h
│   │   ├── simple_cache.h
│   │   └── simple_cache.cpp
│   ├── cpu/
│   │   ├── alu.h / alu.cpp
│   │   ├── csr.h / csr.cpp
│   │   ├── pipeline.h / pipeline.cpp
│   │   ├── registers.h / registers.cpp
│   │   └── single_cycle.h / single_cycle.cpp
│   ├── elf/
│   │   ├── elf_loader.h
│   │   └── elf_loader.cpp
│   ├── isa/
│   │   ├── decoder.h
│   │   └── decoder.cpp
│   ├── mem/
│   │   ├── memory.h
│   │   └── memory.cpp
│   ├── mmu/
│   │   ├── mmu.h
│   │   └── mmu.cpp
│   └── periph/
│       ├── timer.h / timer.cpp
│       ├── timer_mmio.h / timer_mmio.cpp
│       ├── tohost_mmio.h / tohost_mmio.cpp
│       └── uart_mmio.h / uart_mmio.cpp
├── tests/
│   ├── test_cache_basic.cpp
│   ├── test_cache_wb.cpp
│   ├── test_csrs.cpp
│   ├── test_decoder.cpp
│   ├── test_elf_loader.cpp
│   ├── test_integration_uart_timer.cpp
│   ├── test_isa_immediates.cpp
│   ├── test_load_store_byte_half.cpp
│   ├── test_mmu_basic.cpp
│   ├── test_pipeline.cpp
│   ├── test_pipeline_forward.cpp
│   ├── test_pipeline_load_store_instr.cpp
│   ├── test_pipeline_uart_rx.cpp
│   ├── test_privilege_security.cpp
│   ├── test_sfence_vma.cpp
│   ├── test_singlecycle.cpp
│   ├── test_timer_interrupt.cpp
│   ├── test_trap_delegation.cpp
│   └── test_uart_mmio.cpp
└── tmp/
    ├── run_rv32ui_collect.sh
    ├── gen_speedup_report.py
    ├── matmul_cache_run.log
    ├── matmul_nocache_run.log
    ├── test_all_cache.log
    ├── test_all_cache_fixed.log
    ├── test_all_nocache.log
    ├── test_all_nocache_fixed.log
    ├── rv32ui_collect.log
    └── fail_debug/
        └── rv32ui-p-*_*.out

  新增（可视化中间件）：
  - [tools/trace_server.py](tools/trace_server.py)：trace 中间件（SSE + 静态托管）。
  - [tools/run_trace_demo.sh](tools/run_trace_demo.sh)：一键启动 demo（会 spawn `mycpu`）。
  - [web/index.html](web/index.html)、[web/app.js](web/app.js)、[web/style.css](web/style.css)：最小浏览器监视页。
  - [web/README.md](web/README.md)：步骤5前端使用说明。
```

## 文件清单（逐项说明）

### 根目录文件
- [.clang-format](.clang-format)：C/C++ 代码风格配置。
- [.gitignore](.gitignore)：Git 忽略规则。
- [CMakeLists.txt](CMakeLists.txt)：项目构建入口，定义库、可执行与测试目标。
- [README.md](README.md)：项目总览与使用说明。
- [test_all.sh](test_all.sh)：rv32ui 批量执行脚本（支持 CSV 与 cache/no-cache 参数透传）。
- [tmp_min_elf.bin](tmp_min_elf.bin)：最小 ELF/二进制调试样本。

### docs/
- [docs/ISA.md](docs/ISA.md)：指令与实现说明文档。
- [docs/rv32ui_perf.csv](docs/rv32ui_perf.csv)：cache 模式 rv32ui 性能结果。
- [docs/rv32ui_perf_nocache.csv](docs/rv32ui_perf_nocache.csv)：no-cache 模式 rv32ui 性能结果。
- [docs/SPEEDUP_REPORT.md](docs/SPEEDUP_REPORT.md)：cache/no-cache 加速比分析报告。
- [docs/BENCHMARK_REPORT.md](docs/BENCHMARK_REPORT.md)：matmul 基准报告。

### benchmarks/
- [benchmarks/start.S](benchmarks/start.S)：裸机启动代码（栈/段初始化、跳转 main、tohost 退出）。
- [benchmarks/linker.ld](benchmarks/linker.ld)：裸机链接脚本。
- [benchmarks/matmul.c](benchmarks/matmul.c)：16x16 整数矩阵乘 benchmark。
- [benchmarks/matmul.elf](benchmarks/matmul.elf)：matmul 预编译 ELF。
- [benchmarks/matmul.map](benchmarks/matmul.map)：matmul 链接映射文件。
- [benchmarks/hello.c](benchmarks/hello.c)：最小 UART/tohost 演示程序。
- [benchmarks/hello.elf](benchmarks/hello.elf)：hello 预编译 ELF。

### src/

#### src/main.cpp
- [src/main.cpp](src/main.cpp)：CLI 解析、平台初始化、主执行循环、性能输出、异常打印。

#### src/cache/
- [src/cache/cache_config.h](src/cache/cache_config.h)：Cache 参数结构体。
- [src/cache/simple_cache.h](src/cache/simple_cache.h)：SimpleCache 类接口与统计 API。
- [src/cache/simple_cache.cpp](src/cache/simple_cache.cpp)：读写命中/失配、写回/写分配、LRU、flush/invalidate。

#### src/cpu/
- [src/cpu/alu.h](src/cpu/alu.h)、[src/cpu/alu.cpp](src/cpu/alu.cpp)：ALU 运算逻辑。
- [src/cpu/registers.h](src/cpu/registers.h)、[src/cpu/registers.cpp](src/cpu/registers.cpp)：寄存器堆。
- [src/cpu/csr.h](src/cpu/csr.h)、[src/cpu/csr.cpp](src/cpu/csr.cpp)：CSR、特权控制、trap/mret/sret 机制。
- [src/cpu/single_cycle.h](src/cpu/single_cycle.h)、[src/cpu/single_cycle.cpp](src/cpu/single_cycle.cpp)：单周期执行模型（用于教学对照）。
- [src/cpu/pipeline.h](src/cpu/pipeline.h)、[src/cpu/pipeline.cpp](src/cpu/pipeline.cpp)：五级流水线核心（转发、hazard、cache stall、分支、异常）。

#### src/elf/
- [src/elf/elf_loader.h](src/elf/elf_loader.h)、[src/elf/elf_loader.cpp](src/elf/elf_loader.cpp)：ELF 加载、段映射、入口设置、tohost 符号探测。

#### src/isa/
- [src/isa/decoder.h](src/isa/decoder.h)、[src/isa/decoder.cpp](src/isa/decoder.cpp)：指令解码器（opcode/funct 到内部指令名）。

#### src/mem/
- [src/mem/memory.h](src/mem/memory.h)、[src/mem/memory.cpp](src/mem/memory.cpp)：内存模型、MMIO 映射、越界诊断信息。

#### src/mmu/
- [src/mmu/mmu.h](src/mmu/mmu.h)、[src/mmu/mmu.cpp](src/mmu/mmu.cpp)：Sv32 地址翻译、TLB、权限检查、A/D 位更新、SFENCE.VMA。

#### src/periph/
- [src/periph/uart_mmio.h](src/periph/uart_mmio.h)、[src/periph/uart_mmio.cpp](src/periph/uart_mmio.cpp)：UART MMIO 设备。
- [src/periph/timer.h](src/periph/timer.h)、[src/periph/timer.cpp](src/periph/timer.cpp)：定时器模型。
- [src/periph/timer_mmio.h](src/periph/timer_mmio.h)、[src/periph/timer_mmio.cpp](src/periph/timer_mmio.cpp)：定时器 MMIO 接口。
- [src/periph/tohost_mmio.h](src/periph/tohost_mmio.h)、[src/periph/tohost_mmio.cpp](src/periph/tohost_mmio.cpp)：tohost semihost 退出接口。

### tests/
- [tests/test_decoder.cpp](tests/test_decoder.cpp)：解码器测试。
- [tests/test_singlecycle.cpp](tests/test_singlecycle.cpp)：单周期执行测试。
- [tests/test_pipeline.cpp](tests/test_pipeline.cpp)：流水线基础行为测试。
- [tests/test_pipeline_forward.cpp](tests/test_pipeline_forward.cpp)：转发路径测试。
- [tests/test_pipeline_load_store_instr.cpp](tests/test_pipeline_load_store_instr.cpp)：流水线访存指令测试。
- [tests/test_pipeline_uart_rx.cpp](tests/test_pipeline_uart_rx.cpp)：UART RX 与流水线交互测试。
- [tests/test_timer_interrupt.cpp](tests/test_timer_interrupt.cpp)：定时器中断测试。
- [tests/test_integration_uart_timer.cpp](tests/test_integration_uart_timer.cpp)：UART+Timer 集成测试。
- [tests/test_cache_basic.cpp](tests/test_cache_basic.cpp)：缓存基础行为测试。
- [tests/test_cache_wb.cpp](tests/test_cache_wb.cpp)：写回缓存策略测试。
- [tests/test_elf_loader.cpp](tests/test_elf_loader.cpp)：ELF 加载测试。
- [tests/test_mmu_basic.cpp](tests/test_mmu_basic.cpp)：MMU 基础翻译测试。
- [tests/test_sfence_vma.cpp](tests/test_sfence_vma.cpp)：SFENCE.VMA 语义测试。
- [tests/test_csrs.cpp](tests/test_csrs.cpp)：CSR 读写/异常路径测试。
- [tests/test_trap_delegation.cpp](tests/test_trap_delegation.cpp)：trap 委派测试。
- [tests/test_privilege_security.cpp](tests/test_privilege_security.cpp)：特权与权限边界测试。
- [tests/test_isa_immediates.cpp](tests/test_isa_immediates.cpp)：立即数语义测试。
- [tests/test_load_store_byte_half.cpp](tests/test_load_store_byte_half.cpp)：字节/半字访存语义测试。
- [tests/test_uart_mmio.cpp](tests/test_uart_mmio.cpp)：UART MMIO 寄存器语义测试。

### riscv-tests/（外部子模块）
- [riscv-tests](riscv-tests)：官方测试仓库子模块入口（Gitlink）。
- [riscv-tests/isa](riscv-tests/isa)：ISA 测试集目录。
- [riscv-tests/isa/rv32ui-p-add](riscv-tests/isa/rv32ui-p-add) 等 42 个 `rv32ui-p-*`：当前项目批量回归用例。
- [riscv-tests/benchmarks](riscv-tests/benchmarks)、[riscv-tests/debug](riscv-tests/debug)、[riscv-tests/env](riscv-tests/env)、[riscv-tests/mt](riscv-tests/mt)：上游附带子系统（当前项目主要使用 `isa/rv32ui-p-*`）。

### Testing/ 与 build/
- [Testing/Temporary](Testing/Temporary)：CTest 运行时产物。
- [build](build)：CMake 构建产物目录（大量中间文件与测试二进制，不建议入库）。

### tmp/
- [tmp/run_rv32ui_collect.sh](tmp/run_rv32ui_collect.sh)：历史采集脚本。
- [tmp/gen_speedup_report.py](tmp/gen_speedup_report.py)：速度对比报告生成脚本。
- [tmp/matmul_cache_run.log](tmp/matmul_cache_run.log)、[tmp/matmul_nocache_run.log](tmp/matmul_nocache_run.log)：matmul 最新运行日志。
- [tmp/test_all_cache.log](tmp/test_all_cache.log)、[tmp/test_all_nocache.log](tmp/test_all_nocache.log)：中间回归日志。
- [tmp/test_all_cache_fixed.log](tmp/test_all_cache_fixed.log)、[tmp/test_all_nocache_fixed.log](tmp/test_all_nocache_fixed.log)：修复后全量 PASS 日志。
- [tmp/rv32ui_collect.log](tmp/rv32ui_collect.log)：历史性能收集日志。
- [tmp/fail_debug](tmp/fail_debug)：4 个 cache-only 失败项的逐例对比日志目录。


