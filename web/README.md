# Web Visualization (Step 5)

该目录是 Plain HTML + D3 的前端实现，用于展示 myCPU trace：
- 流水线实时视图（IF/ID/EX/MEM/WB）
- 最近 32 cycle 指令时间轴
- 侧边栏性能折线图（最近 1000 cycle 的 IPC 与 D-Cache 命中率）
- x0..x31 寄存器窗口（高亮当周期写回）
- I/D cache 命中率与事件列表
- 当前周期访存事件与原始 trace
- Memory Inspector（按地址窗口回放，随 cycle 更新）
- Virtual Console（从 Trace 的 UART 写入事件提取字符）

## 运行

先启动 trace server：

```bash
cd /home/ys/camycpu
./tools/run_trace_demo.sh
```

然后浏览器访问：

```text
http://localhost:8080
```

## 控制

- `Connect SSE`: 连接 `/events`
- `Pause / Play`: 播放/暂停
- `Step -1 / +1`: 单步回放
- `播放速率`: 调整播放速度
- `Jump`: 跳转到指定 cycle
- `跟随最新`: 打开后自动跟随最新 trace
- `性能反馈`: 实时显示最近 1000 cycle 的 IPC 与 D-Cache 命中率曲线
- `Virtual Console`: 黑底串口终端窗口，按 UART 写入事件追加字符
	- 支持 `\\`、`\n` 等字符渲染（换行/反斜杠按终端显示）
	- `Clear Console` 可清空当前终端内容
- `Memory Inspector`:
	- `起始地址` + `窗口字节` + `Apply` 控制回放窗口
	- `Base = PC` 快速跳到当前 PC 附近（64B 对齐）
	- `Base = Last Store` 一键跳到当前回放位置最近一次写入地址
	- `自动跟随最近写入`：播放/步进时自动把窗口对齐到最近一次写入地址
	- `包含 MMIO` 用于观察 UART/tohost 一类设备地址写入
	- 默认仅回放非 MMIO 的 `SB/SH/SW`；未知字节显示 `..`
	- 若使用 `hello.elf`，常见现象是窗口全 `..`：该程序主要在 MMIO 地址输出字符，可勾选 `包含 MMIO` 或换 `matmul.elf`

## 依赖

- D3 通过 CDN 以 ES module 方式加载（浏览器需要联网）。
