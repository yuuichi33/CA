# myCPU

教学用途的 RISC-V 模拟器（骨架）。

构建：

mkdir -p build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

运行：

./build/mycpu
