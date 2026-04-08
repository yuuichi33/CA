# myCPU

教学用途的 RISC-V 模拟器（骨架）。

构建：

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

运行：

./build/mycpu

运行 riscv-tests (rv32ui)
-------------------------
仓库包含 `riscv-tests` 的 `isa/rv32ui` 预编译测试用例。可以使用项目根目录下的 `test_all.sh` 脚本运行整套测试：

示例：

```bash
# 先构建模拟器
mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# 运行所有 rv32ui 测试（脚本会输出每个测试的 [PASS]/[FAIL]）
./test_all.sh
```

脚本行为：
- 自动扫描 `riscv-tests/isa/rv32ui-p-*` 可执行文件
- 对每个用例使用 `./build/mycpu -l <elf> -c 2000000` 运行（循环次数受限，避免挂起）
- 检测输出中的 `TOHOST:` 字样作为测试通过的信号
- 最后打印通过数量、失败数量与通过率

如果需要改变模拟器路径或循环次数，可直接编辑 `test_all.sh` 顶部的 `EMULATOR` / `CYCLES` 变量。
