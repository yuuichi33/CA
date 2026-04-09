# Trace Server (WSL1)

`tools/trace_server.py` 是一个零依赖（Python 标准库）的 Trace 中间件：
- 运行 `mycpu` 子进程并读取 `--trace-json stdout`。
- 将每行 JSONL 通过 SSE (`/events`) 推送到浏览器。
- 托管 `web/` 静态页面。

## 快速开始

```bash
cd /home/ys/camycpu
chmod +x tools/run_trace_demo.sh
./tools/run_trace_demo.sh
```

浏览器打开：
- `http://localhost:8080`

## 手动命令

```bash
cd /home/ys/camycpu
python3 tools/trace_server.py --host 0.0.0.0 --port 8080 --web-root web -- ./build/mycpu --load benchmarks/hello.elf --trace-json stdout --max-cycles 2000
```

## 指定展示程序

默认展示 `benchmarks/hello.elf`。可通过脚本参数切换到任意 ELF（例如 matmul）：

```bash
cd /home/ys/camycpu
./tools/run_trace_demo.sh benchmarks/matmul.elf 5000
```

## 健康检查

```bash
curl -s http://localhost:8080/health
```
