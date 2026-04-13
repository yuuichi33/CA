# Visual Demo Guide（Stall 动画与 Cache 命中率）

本指南用于现场演示 Web 可视化页面，重点展示：
1. 流水线 STALL 动画
2. D-Cache 命中率趋势与 Cache 事件

## 1. 演示前准备

```bash
cd /home/ys/camycpu
chmod +x tools/run_trace_demo.sh
./tools/run_trace_demo.sh benchmarks/quicksort_stress.elf 200000
```

启动后按终端输出打开浏览器地址（示例：`http://127.0.0.1:18080`）。

## 2. 页面关键区域（按讲解顺序）

1. 控制台（左上）
- 按钮：Connect SSE / Pause / Step +/- / Jump
- 用途：演示实时流与逐帧回放

2. 流水线实时视图（Pipeline）
- 现象：遇到停顿时出现明显的 `STALL CYCLE` 标记
- 话术：说明 cache miss 或 hazard 造成停顿，不是程序错误

3. 性能反馈折线图（最近 1000 cycle）
- 三条线：IPC、D-Cache Hit、Cache Stall
- 重点：当 Cache Stall 抬升时，IPC 通常下降

4. Cache 观察面板
- I-Cache / D-Cache 命中率条形图
- 最近 Cache 事件列表（hit/miss + 地址）
- 话术：从“事件级”解释命中率变化原因

5. 时间轴（最近 32 cycle）
- 强调 stall 周期的密度变化

## 3. 推荐演示脚本（约 5 分钟）

1. 第 0-1 分钟：连接与基础状态
- 点击 Connect SSE，确认 `连接` 为 connected
- 指出 Cycle、PC 在推进

2. 第 1-2 分钟：演示 STALL 动画
- 暂停后单步 `Step +1`
- 当出现 STALL CYCLE 时，说明“该周期流水线被拉住”

3. 第 2-4 分钟：演示 Cache 命中率与事件
- 观察 D-Cache 命中率条变化
- 对照“最近 Cache 事件”中 miss 记录
- 指出 miss 聚集区通常对应 Stall 线抬升

4. 第 4-5 分钟：结论与回到门禁数据
- 回扣 `docs/cache_matrix/<run-tag>/policy_summary.csv`
- 强调可视化现象与门禁统计（hit/stall）一致

## 4. 常见故障与兜底

1. 页面不更新
- 操作：点击 Connect SSE 重连
- 检查：终端是否仍在输出 trace

2. 看不到明显 stall
- 切换 workload：优先用 `quicksort_stress.elf`
- 使用 Step 模式放大局部现象

3. 命中率长期为 0
- 检查是否误用 no-cache 路径
- 检查运行命令是否包含 `--no-cache`

4. 串口无输出不影响本次演示
- 本指南主线是 stall 与 cache，不依赖 Virtual Console 文本输出
