# myCPU

myCPU 是一个用 C++ 实现的教学用 RISC‑V 模拟器骨架，目标是作为研究与教学的试验平台。
它实现了一个带 MMU 与简单外设的五级流水线 RV32I 仿真器，易于阅读与扩展。

核心定位
----------------
- 语言：C++17
- 架构：RISC‑V RV32I（用户可在代码中扩展指令集）
- 微架构：五级流水线（IF / ID / EX / MEM / WB），带数据转发和流水线挂起逻辑
- 面向：教学、课程演示、ISA/微架构实验

核心特性审计（基于当前代码实现）
---------------------------------

- 微架构
	- 五级流水线：`IF/ID/EX/MEM/WB`，实现了流水线寄存器与控制逻辑（参见 [src/cpu/pipeline.cpp](src/cpu/pipeline.cpp#L1)）。
	- 实现了完整的数据转发（forwarding）路径与 Load‑Use hazard 检测以生成 Stall。

- 指令集
	- 实现 RV32I 整数子集（常用算术/逻辑/载入存储/分支/跳转/CSR 指令）。
	- 已使用官方 `riscv-tests` 的 `rv32ui` 子集进行验证（见下文测试章节）。

- 特权态与异常
	- 支持 M/S/U 三种特权模式，并实现了 CSR 读写与 Trap 管理。
	- 支持异常/中断委派（`medeleg` / `mideleg`）与 `mret`/`sret` 恢复流程（参见 [src/cpu/csr.cpp](src/cpu/csr.cpp#L1)）。

- 内存系统
	- Sv32 两级页面表实现（PTE 验证、A/D 位更新等，见 [src/mmu/mmu.cpp](src/mmu/mmu.cpp#L1)）。
	- 小型 4‑entry 全相联 TLB（含 ASID 支持）与基本 I/D cache（教学实现）。

- 外设
	- UART（MMIO）—用于字符串 IO 与测试注入。
	- Timer（MMIO）—可产生定时中断。
	- Semihost `tohost` 支持：ELF loader 会检测 `tohost` 符号并自动映射 `ToHostMMIO`（见 [src/elf/elf_loader.cpp](src/elf/elf_loader.cpp#L1) 与 [src/periph/tohost_mmio.cpp](src/periph/tohost_mmio.cpp#L1)）。

测试指南（重点）
-----------------

本项目包含两类测试：内部单元测试（单测）与官方 ISA 测试套件（`riscv-tests` 的 `rv32ui`）。

内部单测
^^^^^^^^^^^

构建项目后可使用 `ctest` 或手动运行位于 `build/` 下的测试二进制（名称以 `test_` 开头）。

构建并运行：

```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# 使用 ctest
cd build
ctest --output-on-failure

# 或手动运行所有测试二进制
for t in ./test_*; do [ -x "$t" ] || continue; echo "=== $t ==="; "$t" || exit 1; done
```

当前仓库包含 16+ 个单元测试，覆盖译码、转发、流水线、MMU、TLB、CSR/特权、安全性检查、外设集成与 SFENCE.VMA 等（测试文件位于 `tests/`，构建结果在 `build/`）。

官方 ISA 测试（rv32ui）
^^^^^^^^^^^^^^^^^^^^^^^^

仓库包含预编译的 `riscv-tests/isa/rv32ui-p-*` 用例。为便捷执行，我提供了 `test_all.sh` 脚本：

```bash
# 构建模拟器（如尚未构建）：
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# 运行所有 rv32ui 用例：
./test_all.sh
```

脚本行为要点：
- 自动扫描 `riscv-tests/isa/rv32ui-p-*` 可执行文件。
- 对每个用例运行 `./build/mycpu -l <elf> -c 2000000`，`-c` 为循环/指令上限以防挂起。
- 将在仿真输出中搜索 `TOHOST:` 字样以判定用例通过（`riscv-tests` 通过 `tohost`/`fromhost` 实现 semihosting 风格的通过信号）。
- 脚本最后会打印通过/失败统计与通过率。

示例（项目内已验证）：

```
Running 42 tests from riscv-tests/isa/rv32ui-*
...
================ Test Summary ================
Total:    42
Passed:   42
Failed:   0
Pass rate: 100%
```

快速上手（摘要）
-----------------

系统与依赖：

- 推荐开发环境：WSL2 + Ubuntu 22.04 或任意现代 Linux。 
- 需要工具：`cmake`, `make`/`ninja`, `build-essential`。
- 可选（用于分析/反汇编）：RISC‑V GNU 工具链（例如 `riscv64-unknown-elf-gcc` / `objdump` / `readelf`）。

安装示例（Ubuntu）：

```bash
sudo apt update
sudo apt install -y build-essential cmake git
# 安装 riscv 工具链（视平台与包源而定）
# sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
```

构建：

```bash
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

运行示例：

```bash
# 以 ELF 文件作为输入并开启详细日志
./build/mycpu -l path/to/program.elf -v -c 2000000
```

常用选项说明：

- `-l, --load <file>`：加载 ELF
- `-c, --cycles <n>`：执行上限（指令/循环），防止测试挂起
- `-v, --verbose`：打印详细日志（用于调试）
- `--step`：逐步执行（单步）

开发者指南 / 代码位置
-----------------------

- 流水线与控制：`src/cpu/pipeline.cpp`
- CSR 与特权：`src/cpu/csr.cpp`、`src/cpu/csr.h`
- MMU / TLB：`src/mmu/mmu.cpp`、`src/mmu/mmu.h`
- ELF 加载与符号检测（tohost）：`src/elf/elf_loader.cpp`
- Semihost/tohost 实现：`src/periph/tohost_mmio.cpp`

贡献 & 联系
------------

欢迎提交 Issue 或 PR。建议在 PR 中包含复现步骤、测试日志以及所修改的目标模块单元测试。

许可
----
请查阅仓库根目录的 LICENSE 文件（如无，请在使用前添加合适许可证）。

感谢使用 myCPU —— 这是一个面向教学与原型验证的简洁模拟器实现，欢迎改进与扩展。
