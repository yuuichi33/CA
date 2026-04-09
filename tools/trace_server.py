#!/usr/bin/env python3
"""Lightweight trace server for myCPU (WSL1 friendly, stdlib-only).

Features:
- Spawn mycpu process and stream its JSONL stdout trace.
- Broadcast trace lines over Server-Sent Events at /events.
- Serve static files (default: ../web).
- Expose /health for quick diagnostics.
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import queue
import signal
import subprocess
import sys
import threading
import time
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Deque, List, Optional


class TraceHub:
    def __init__(self, max_buffer_lines: int = 2000) -> None:
        self._buffer: Deque[str] = collections.deque(maxlen=max_buffer_lines)
        self._clients: List[queue.Queue[str]] = []
        self._lock = threading.Lock()
        self._line_count = 0
        self._last_cycle = -1

    def snapshot(self) -> List[str]:
        with self._lock:
            return list(self._buffer)

    def register_client(self) -> queue.Queue[str]:
        q: queue.Queue[str] = queue.Queue(maxsize=2048)
        with self._lock:
            self._clients.append(q)
        return q

    def unregister_client(self, q: queue.Queue[str]) -> None:
        with self._lock:
            if q in self._clients:
                self._clients.remove(q)

    def publish(self, line: str) -> None:
        if not line:
            return

        # Keep lightweight stats for /health.
        self._line_count += 1
        try:
            obj = json.loads(line)
            cycle = obj.get("cycle")
            if isinstance(cycle, int):
                self._last_cycle = cycle
        except Exception:
            pass

        with self._lock:
            self._buffer.append(line)
            stale: List[queue.Queue[str]] = []
            for q in self._clients:
                try:
                    q.put_nowait(line)
                except queue.Full:
                    stale.append(q)
            for q in stale:
                if q in self._clients:
                    self._clients.remove(q)

    def health(self, child_pid: Optional[int], child_running: bool) -> dict:
        with self._lock:
            clients = len(self._clients)
            buffered = len(self._buffer)
        return {
            "ok": True,
            "clients": clients,
            "buffered_lines": buffered,
            "total_lines": self._line_count,
            "last_cycle": self._last_cycle,
            "child_pid": child_pid,
            "child_running": child_running,
            "ts": int(time.time()),
        }


def sse_write(handler: SimpleHTTPRequestHandler, line: str) -> None:
    payload = f"data: {line}\n\n".encode("utf-8", errors="replace")
    handler.wfile.write(payload)
    handler.wfile.flush()


def make_handler(hub: TraceHub, web_root: Path, child_ref: dict):
    class Handler(SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(web_root), **kwargs)

        def log_message(self, fmt: str, *args):
            sys.stderr.write("[http] " + (fmt % args) + "\n")

        def do_GET(self):
            if self.path == "/health":
                self._serve_health()
                return
            if self.path == "/events":
                self._serve_events()
                return
            super().do_GET()

        def _serve_health(self):
            health = hub.health(
                child_ref.get("pid"),
                bool(child_ref.get("running", False)),
            )
            body = json.dumps(health, ensure_ascii=True).encode("utf-8")
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _serve_events(self):
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.end_headers()

            q = hub.register_client()
            try:
                # Replay buffered trace lines for newly connected clients.
                for line in hub.snapshot():
                    sse_write(self, line)

                while True:
                    try:
                        line = q.get(timeout=10)
                        sse_write(self, line)
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                pass
            finally:
                hub.unregister_client(q)

    return Handler


def spawn_child(cmd: List[str], hub: TraceHub, child_ref: dict, cwd: Path) -> subprocess.Popen:
    proc = subprocess.Popen(
        cmd,
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
    )

    child_ref["pid"] = proc.pid
    child_ref["running"] = True

    def pump_stdout() -> None:
        assert proc.stdout is not None
        for raw in proc.stdout:
            hub.publish(raw.rstrip("\n"))

    def pump_stderr() -> None:
        assert proc.stderr is not None
        for raw in proc.stderr:
            sys.stderr.write("[child] " + raw)

    threading.Thread(target=pump_stdout, daemon=True).start()
    threading.Thread(target=pump_stderr, daemon=True).start()

    def wait_child() -> None:
        code = proc.wait()
        child_ref["running"] = False
        hub.publish(json.dumps({"kind": "meta", "event": "process_exit", "code": code}))
        sys.stderr.write(f"[trace-server] child exited code={code}\n")

    threading.Thread(target=wait_child, daemon=True).start()
    return proc


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="myCPU trace SSE server (WSL1 friendly)")
    parser.add_argument("--host", default="0.0.0.0", help="Bind host (default: 0.0.0.0 for WSL1)")
    parser.add_argument("--port", type=int, default=8080, help="HTTP port")
    parser.add_argument(
        "--web-root",
        default="web",
        help="Static web root (default: web)",
    )
    parser.add_argument(
        "--buffer-lines",
        type=int,
        default=2000,
        help="Replay buffer line count for new clients",
    )
    parser.add_argument(
        "cmd",
        nargs=argparse.REMAINDER,
        help="Command to spawn after '--', e.g. -- ./build/mycpu --load ... --trace-json stdout",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    web_root = Path(args.web_root)
    if not web_root.is_absolute():
        web_root = repo_root / web_root

    if not web_root.exists():
        sys.stderr.write(f"[trace-server] web root not found: {web_root}\n")
        return 2

    cmd = list(args.cmd)
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]

    hub = TraceHub(max_buffer_lines=args.buffer_lines)
    child_ref = {"pid": None, "running": False}
    child_proc: Optional[subprocess.Popen] = None

    if cmd:
        sys.stderr.write(f"[trace-server] spawn: {' '.join(cmd)}\n")
        child_proc = spawn_child(cmd, hub, child_ref, repo_root)
    else:
        sys.stderr.write("[trace-server] no child command provided, serving static/SSE only\n")

    handler = make_handler(hub, web_root, child_ref)
    server = ThreadingHTTPServer((args.host, args.port), handler)

    stop_event = threading.Event()

    def shutdown(*_):
        if stop_event.is_set():
            return
        stop_event.set()
        sys.stderr.write("[trace-server] shutting down...\n")
        server.shutdown()
        if child_proc and child_proc.poll() is None:
            child_proc.terminate()

    signal.signal(signal.SIGINT, shutdown)
    signal.signal(signal.SIGTERM, shutdown)

    sys.stderr.write(
        f"[trace-server] listening http://{args.host}:{args.port} (web_root={web_root})\n"
    )

    try:
        server.serve_forever(poll_interval=0.2)
    finally:
        server.server_close()
        if child_proc and child_proc.poll() is None:
            child_proc.terminate()
            try:
                child_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                child_proc.kill()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
