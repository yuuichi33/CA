#!/usr/bin/env python3
import argparse
import csv
import json
import math
import os
import re
import statistics
import zlib
from datetime import date

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REPORT_MD = os.path.join(ROOT, "docs", "FULL_TEST_PERFORMANCE_REPORT.md")

# 3x5 bitmap font for lightweight text rendering without external dependencies.
FONT_3X5 = {
    " ": ("000", "000", "000", "000", "000"),
    "-": ("000", "000", "111", "000", "000"),
    "_": ("000", "000", "000", "000", "111"),
    ".": ("000", "000", "000", "000", "010"),
    ",": ("000", "000", "000", "010", "100"),
    ":": ("000", "010", "000", "010", "000"),
    "/": ("001", "001", "010", "100", "100"),
    "%": ("101", "001", "010", "100", "101"),
    "(": ("011", "100", "100", "100", "011"),
    ")": ("110", "001", "001", "001", "110"),
    "=": ("000", "111", "000", "111", "000"),
    "+": ("000", "010", "111", "010", "000"),
    "?": ("111", "001", "011", "000", "010"),
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "001", "001", "001"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
    "A": ("111", "101", "111", "101", "101"),
    "B": ("110", "101", "110", "101", "110"),
    "C": ("111", "100", "100", "100", "111"),
    "D": ("110", "101", "101", "101", "110"),
    "E": ("111", "100", "110", "100", "111"),
    "F": ("111", "100", "110", "100", "100"),
    "G": ("111", "100", "101", "101", "111"),
    "H": ("101", "101", "111", "101", "101"),
    "I": ("111", "010", "010", "010", "111"),
    "J": ("001", "001", "001", "101", "111"),
    "K": ("101", "101", "110", "101", "101"),
    "L": ("100", "100", "100", "100", "111"),
    "M": ("101", "111", "111", "101", "101"),
    "N": ("101", "111", "111", "111", "101"),
    "O": ("111", "101", "101", "101", "111"),
    "P": ("111", "101", "111", "100", "100"),
    "Q": ("111", "101", "101", "111", "001"),
    "R": ("111", "101", "111", "110", "101"),
    "S": ("111", "100", "111", "001", "111"),
    "T": ("111", "010", "010", "010", "010"),
    "U": ("101", "101", "101", "101", "111"),
    "V": ("101", "101", "101", "101", "010"),
    "W": ("101", "101", "111", "111", "101"),
    "X": ("101", "101", "010", "101", "101"),
    "Y": ("101", "101", "010", "010", "010"),
    "Z": ("111", "001", "010", "100", "111"),
}


COL_BG = (248, 250, 252)
COL_AXIS = (70, 80, 95)
COL_GRID = (224, 229, 236)
COL_TEXT = (28, 34, 42)
COL_BLUE = (72, 118, 184)
COL_ORANGE = (219, 131, 62)
COL_GREEN = (90, 160, 90)
COL_PURPLE = (175, 88, 170)
COL_RED = (200, 60, 60)
COL_GRAY = (120, 120, 120)


def parse_args():
    today_tag = date.today().strftime("%Y%m%d")
    parser = argparse.ArgumentParser(description="Generate full test report and PNG figures.")
    parser.add_argument("--run-tag", default=today_tag, help="Tag used in filenames, default YYYYMMDD.")
    parser.add_argument("--run-dir", default="", help="Input run directory, default tmp/full_run_<run-tag>.")
    parser.add_argument("--report-date", default=date.today().isoformat(), help="Date shown in report header.")
    parser.add_argument(
        "--cache-matrix-dir",
        default="",
        help="Optional cache matrix dir, default docs/cache_matrix/<run-tag> when present.",
    )
    parser.add_argument(
        "--gate-prefix",
        default="",
        help="Optional gate output prefix (without _result.json suffix).",
    )
    parser.add_argument(
        "--benchmark-dir",
        default="",
        help="Optional benchmark dir, default docs/benchmark/<run-tag> when present.",
    )
    parser.add_argument(
        "--benchmark-gate-prefix",
        default="",
        help="Optional benchmark gate prefix (without _result.json suffix).",
    )
    return parser.parse_args()


def build_paths(run_tag, run_dir, cache_matrix_dir, gate_prefix, benchmark_dir, benchmark_gate_prefix):
    resolved_run_dir = run_dir or os.path.join(ROOT, "tmp", f"full_run_{run_tag}")
    resolved_matrix_dir = cache_matrix_dir
    if not resolved_matrix_dir:
        guess_dir = os.path.join(ROOT, "docs", "cache_matrix", run_tag)
        if os.path.isdir(guess_dir):
            resolved_matrix_dir = guess_dir
    resolved_gate_prefix = gate_prefix
    if not resolved_gate_prefix and resolved_matrix_dir:
        resolved_gate_prefix = os.path.join(resolved_matrix_dir, "gate")

    resolved_benchmark_dir = benchmark_dir
    if not resolved_benchmark_dir:
        guess_bench_dir = os.path.join(ROOT, "docs", "benchmark", run_tag)
        if os.path.isdir(guess_bench_dir):
            resolved_benchmark_dir = guess_bench_dir

    resolved_benchmark_gate_prefix = benchmark_gate_prefix
    if not resolved_benchmark_gate_prefix and resolved_benchmark_dir:
        resolved_benchmark_gate_prefix = os.path.join(resolved_benchmark_dir, "benchmark_gate")

    fig_dir = os.path.join(ROOT, "docs", "figures")
    return {
        "run_tag": run_tag,
        "run_dir": resolved_run_dir,
        "matrix_dir": resolved_matrix_dir,
        "benchmark_dir": resolved_benchmark_dir,
        "p1_csv": os.path.join(ROOT, "docs", "rv32ui_perf_full_p1.csv"),
        "p10_csv": os.path.join(ROOT, "docs", "rv32ui_perf_full_p10.csv"),
        "nc_csv": os.path.join(ROOT, "docs", "rv32ui_perf_full_nocache.csv"),
        "ctest_log": os.path.join(resolved_run_dir, "ctest_full.log"),
        "bench_rc": os.path.join(resolved_run_dir, "benchmark_rcs.csv"),
        "web_health": os.path.join(resolved_run_dir, "web_health.json"),
        "web_index_head": os.path.join(resolved_run_dir, "web_index_head.txt"),
        "web_smoke_status": os.path.join(resolved_run_dir, "web_smoke_status.json"),
        "disk_core": os.path.join(resolved_run_dir, "disk_usage_core.txt"),
        "disk_top": os.path.join(resolved_run_dir, "disk_usage_top10.txt"),
        "fig_dir": fig_dir,
        "report_md": REPORT_MD,
        "summary_csv": os.path.join(ROOT, "docs", f"full_test_summary_{run_tag}.csv"),
        "speedup_fig": os.path.join(fig_dir, f"full_run_{run_tag}_speedup_bar.png"),
        "scatter_fig": os.path.join(fig_dir, f"full_run_{run_tag}_hitrate_scatter.png"),
        "bench_fig": os.path.join(fig_dir, f"full_run_{run_tag}_benchmark_cycles_log.png"),
        "matrix_summary": os.path.join(resolved_matrix_dir, "policy_summary.csv") if resolved_matrix_dir else "",
        "matrix_detail": os.path.join(resolved_matrix_dir, "matrix_detail.csv") if resolved_matrix_dir else "",
        "benchmark_matrix_summary": os.path.join(resolved_matrix_dir, "benchmark_policy_summary.csv") if resolved_matrix_dir else "",
        "benchmark_matrix_detail": os.path.join(resolved_matrix_dir, "benchmark_matrix_detail.csv") if resolved_matrix_dir else "",
        "gate_checks": (resolved_gate_prefix + "_checks.csv") if resolved_gate_prefix else "",
        "gate_result": (resolved_gate_prefix + "_result.json") if resolved_gate_prefix else "",
        "gate_report": (resolved_gate_prefix + "_report.md") if resolved_gate_prefix else "",
        "benchmark_cache_gate_checks": os.path.join(resolved_matrix_dir, "benchmark_cache_gate_checks.csv") if resolved_matrix_dir else "",
        "benchmark_cache_gate_result": os.path.join(resolved_matrix_dir, "benchmark_cache_gate_result.json") if resolved_matrix_dir else "",
        "benchmark_cache_gate_report": os.path.join(resolved_matrix_dir, "benchmark_cache_gate_report.md") if resolved_matrix_dir else "",
        "benchmark_p1": os.path.join(resolved_benchmark_dir, "benchmark_p1.csv") if resolved_benchmark_dir else "",
        "benchmark_p10": os.path.join(resolved_benchmark_dir, "benchmark_p10.csv") if resolved_benchmark_dir else "",
        "benchmark_nocache": os.path.join(resolved_benchmark_dir, "benchmark_nocache.csv") if resolved_benchmark_dir else "",
        "benchmark_detail": os.path.join(resolved_benchmark_dir, "benchmark_detail.csv") if resolved_benchmark_dir else "",
        "benchmark_summary": os.path.join(resolved_benchmark_dir, "benchmark_summary.csv") if resolved_benchmark_dir else "",
        "benchmark_gate_checks": (resolved_benchmark_gate_prefix + "_checks.csv") if resolved_benchmark_gate_prefix else "",
        "benchmark_gate_result": (resolved_benchmark_gate_prefix + "_result.json") if resolved_benchmark_gate_prefix else "",
        "benchmark_gate_report": (resolved_benchmark_gate_prefix + "_report.md") if resolved_benchmark_gate_prefix else "",
        "bench_logs": {
            "hello_default": os.path.join(resolved_run_dir, "hello_default.log"),
            "matmul_cache_p10": os.path.join(resolved_run_dir, "matmul_cache_p10.log"),
            "matmul_nocache_p10": os.path.join(resolved_run_dir, "matmul_nocache_p10.log"),
            "quicksort_cache_default": os.path.join(resolved_run_dir, "quicksort_cache_default.log"),
            "quicksort_nocache": os.path.join(resolved_run_dir, "quicksort_nocache.log"),
            "quicksort_writethrough_p1": os.path.join(resolved_run_dir, "quicksort_writethrough_p1.log"),
        },
    }


def read_csv_rows(path):
    out = {}
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            out[row["test"]] = row
    return out


def to_int(v, d=0):
    try:
        return int(float(v))
    except Exception:
        return d


def to_float(v, d=0.0):
    try:
        return float(v)
    except Exception:
        return d


def percentile(vals, p):
    if not vals:
        return 0.0
    s = sorted(vals)
    if len(s) == 1:
        return float(s[0])
    k = (len(s) - 1) * p
    lo = int(k)
    hi = min(lo + 1, len(s) - 1)
    frac = k - lo
    return s[lo] * (1.0 - frac) + s[hi] * frac


def geomean(vals):
    xs = [v for v in vals if v > 0]
    if not xs:
        return 0.0
    return math.exp(sum(math.log(v) for v in xs) / len(xs))


def corr(xs, ys):
    if len(xs) != len(ys) or len(xs) < 2:
        return 0.0
    mx = statistics.mean(xs)
    my = statistics.mean(ys)
    num = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    denx = sum((x - mx) ** 2 for x in xs)
    deny = sum((y - my) ** 2 for y in ys)
    den = math.sqrt(denx * deny)
    return 0.0 if den == 0 else num / den


def is_mem_test(name):
    n = name.replace("rv32ui-p-", "")
    return n in {
        "lb",
        "lbu",
        "lh",
        "lhu",
        "lw",
        "sb",
        "sh",
        "sw",
        "ld_st",
        "st_ld",
        "ma_data",
    }


def parse_ctest(log_path):
    if not log_path or not os.path.exists(log_path):
        return {"pass_pct": 0, "failed": -1, "total": -1}
    with open(log_path, "r", encoding="utf-8") as f:
        txt = f.read()
    m = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", txt)
    if not m:
        return {"pass_pct": 0, "failed": -1, "total": -1}
    return {
        "pass_pct": int(m.group(1)),
        "failed": int(m.group(2)),
        "total": int(m.group(3)),
    }


def parse_bench_rcs(path):
    if not path or not os.path.exists(path):
        return {}
    out = {}
    with open(path, newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            if len(row) < 2:
                continue
            out[row[0]] = int(row[1])
    return out


def parse_perf_line(log_path):
    data = {
        "cycles": 0,
        "instrs": 0,
        "i_hit": 0.0,
        "d_hit": 0.0,
        "stall": 0,
        "cache_stall": 0,
        "hazard_stall": 0,
        "checksum": "",
    }
    if not log_path or not os.path.exists(log_path):
        return data
    with open(log_path, "r", encoding="utf-8") as f:
        lines = [ln.strip() for ln in f if ln.strip()]
    for ln in lines:
        if "checksum=0x" in ln:
            data["checksum"] = ln.split("checksum=0x", 1)[1].strip()
        if ln.startswith("Cycles:"):
            m = re.search(
                r"Cycles:\s*(\d+),\s*Instrs:\s*(\d+),\s*I-Cache Hit:\s*([0-9.]+)%,\s*D-Cache Hit:\s*([0-9.]+)%.*Stall:\s*(\d+),\s*CacheStall:\s*(\d+),\s*HazardStall:\s*(\d+)",
                ln,
            )
            if m:
                data["cycles"] = int(m.group(1))
                data["instrs"] = int(m.group(2))
                data["i_hit"] = float(m.group(3))
                data["d_hit"] = float(m.group(4))
                data["stall"] = int(m.group(5))
                data["cache_stall"] = int(m.group(6))
                data["hazard_stall"] = int(m.group(7))
    return data


def make_canvas(w, h, bg=COL_BG):
    pix = bytearray(w * h * 3)
    for y in range(h):
        for x in range(w):
            i = (y * w + x) * 3
            pix[i:i + 3] = bytes(bg)
    return {"w": w, "h": h, "pix": pix}


def set_px(c, x, y, color):
    if x < 0 or y < 0 or x >= c["w"] or y >= c["h"]:
        return
    i = (y * c["w"] + x) * 3
    c["pix"][i:i + 3] = bytes(color)


def fill_rect(c, x0, y0, x1, y1, color):
    xa, xb = sorted((int(x0), int(x1)))
    ya, yb = sorted((int(y0), int(y1)))
    xa = max(0, xa)
    ya = max(0, ya)
    xb = min(c["w"] - 1, xb)
    yb = min(c["h"] - 1, yb)
    for y in range(ya, yb + 1):
        row = y * c["w"]
        for x in range(xa, xb + 1):
            i = (row + x) * 3
            c["pix"][i:i + 3] = bytes(color)


def draw_line(c, x0, y0, x1, y1, color):
    x0, y0, x1, y1 = int(x0), int(y0), int(x1), int(y1)
    dx = abs(x1 - x0)
    sx = 1 if x0 < x1 else -1
    dy = -abs(y1 - y0)
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    while True:
        set_px(c, x0, y0, color)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x0 += sx
        if e2 <= dx:
            err += dx
            y0 += sy


def fill_circle(c, cx, cy, r, color):
    cx = int(cx)
    cy = int(cy)
    r = int(r)
    for y in range(cy - r, cy + r + 1):
        for x in range(cx - r, cx + r + 1):
            if (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r:
                set_px(c, x, y, color)


def text_width(text, scale=2, spacing=1):
    lines = text.split("\n")
    adv = (3 + spacing) * scale
    return max((len(line) * adv for line in lines), default=0)


def draw_text(c, x, y, text, color=COL_TEXT, scale=2, spacing=1):
    ox = int(x)
    cx = ox
    cy = int(y)
    adv = (3 + spacing) * scale
    line_h = 5 * scale + scale
    for ch in text:
        if ch == "\n":
            cx = ox
            cy += line_h
            continue
        glyph = FONT_3X5.get(ch.upper(), FONT_3X5["?"])
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit == "1":
                    fill_rect(
                        c,
                        cx + gx * scale,
                        cy + gy * scale,
                        cx + (gx + 1) * scale - 1,
                        cy + (gy + 1) * scale - 1,
                        color,
                    )
        cx += adv


def write_png(path, canvas):
    w = canvas["w"]
    h = canvas["h"]
    pix = canvas["pix"]

    def chunk(tag, data):
        tagb = tag.encode("ascii")
        crc = zlib.crc32(tagb)
        crc = zlib.crc32(data, crc) & 0xFFFFFFFF
        return len(data).to_bytes(4, "big") + tagb + data + crc.to_bytes(4, "big")

    raw = bytearray()
    row_bytes = w * 3
    for y in range(h):
        raw.append(0)
        start = y * row_bytes
        raw.extend(pix[start:start + row_bytes])

    ihdr = w.to_bytes(4, "big") + h.to_bytes(4, "big") + bytes([8, 2, 0, 0, 0])
    idat = zlib.compress(bytes(raw), level=9)
    png = b"\x89PNG\r\n\x1a\n" + chunk("IHDR", ihdr) + chunk("IDAT", idat) + chunk("IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def render_speedup_bar(path, rows):
    c = make_canvas(1400, 720)
    left, right, top, bottom = 130, 40, 100, 140
    pw = c["w"] - left - right
    ph = c["h"] - top - bottom

    draw_text(c, left, 24, "RV32UI SPEEDUP DISTRIBUTION", scale=4)
    draw_text(c, left, 52, "PROFILE: P10 CACHE VS NO-CACHE", scale=2, color=(50, 72, 98))

    draw_line(c, left, top + ph, left + pw, top + ph, COL_AXIS)
    draw_line(c, left, top, left, top + ph, COL_AXIS)

    max_v = max((r["speedup_p10"] for r in rows), default=1.0)
    max_v = max(max_v, 1.0)
    avg_v = statistics.mean([r["speedup_p10"] for r in rows]) if rows else 0.0

    y_ticks = 5
    for i in range(y_ticks + 1):
        v = max_v * i / y_ticks
        y = top + ph - int((v / max_v) * (ph - 1))
        draw_line(c, left - 5, y, left, y, COL_AXIS)
        draw_line(c, left + 1, y, left + pw, y, COL_GRID)
        draw_text(c, 18, y - 6, f"{v:.1f}", scale=2, color=COL_AXIS)

    n = max(1, len(rows))
    step = pw / n
    bar_w = max(2, int(step * 0.72))

    for i, r in enumerate(rows):
        x0 = int(left + i * step + (step - bar_w) / 2)
        x1 = x0 + bar_w
        h = int((r["speedup_p10"] / max_v) * (ph - 1)) if max_v > 0 else 0
        y0 = top + ph - h
        color = COL_ORANGE if r["mem_test"] else COL_BLUE
        fill_rect(c, x0, y0, x1, top + ph - 1, color)

    y_avg = top + ph - int((avg_v / max_v) * (ph - 1)) if max_v > 0 else top + ph
    draw_line(c, left, y_avg, left + pw, y_avg, COL_RED)
    draw_text(c, left + pw - 210, y_avg - 16, f"MEAN={avg_v:.2f}", scale=2, color=COL_RED)

    tick_indices = sorted(set([0, n // 4, n // 2, (3 * n) // 4, n - 1]))
    for idx in tick_indices:
        x = left + int((idx + 0.5) * step)
        draw_line(c, x, top + ph, x, top + ph + 6, COL_AXIS)
        draw_text(c, x - 8, top + ph + 14, str(idx + 1), scale=2, color=COL_AXIS)

    draw_text(c, left + (pw - text_width("X: TEST INDEX (SORTED BY SPEEDUP)", 2)) // 2, top + ph + 56,
              "X: TEST INDEX (SORTED BY SPEEDUP)", scale=2, color=COL_AXIS)
    draw_text(c, 12, top + ph // 2 - 10, "Y: SPEEDUP (X)", scale=2, color=COL_AXIS)

    lx, ly = left + pw - 250, top + 26
    fill_rect(c, lx, ly, lx + 18, ly + 12, COL_BLUE)
    draw_text(c, lx + 28, ly + 2, "NON-MEM", scale=2)
    fill_rect(c, lx, ly + 24, lx + 18, ly + 36, COL_ORANGE)
    draw_text(c, lx + 28, ly + 26, "MEM", scale=2)
    draw_line(c, lx, ly + 50, lx + 18, ly + 50, COL_RED)
    draw_text(c, lx + 28, ly + 44, "MEAN", scale=2)

    write_png(path, c)


def render_scatter(path, rows):
    c = make_canvas(980, 720)
    left, right, top, bottom = 110, 70, 100, 140
    pw = c["w"] - left - right
    ph = c["h"] - top - bottom

    draw_text(c, left, 24, "CACHE HIT RATE VS SPEEDUP", scale=4)
    draw_text(c, left, 52, "BLUE=D-HIT  ORANGE=I-HIT", scale=2, color=(50, 72, 98))

    draw_line(c, left, top + ph, left + pw, top + ph, COL_AXIS)
    draw_line(c, left, top, left, top + ph, COL_AXIS)

    max_y = max((r["speedup_p10"] for r in rows), default=1.0)
    max_y = max(max_y, 1.0)

    for x_pct in range(0, 101, 20):
        x = left + int((x_pct / 100.0) * pw)
        draw_line(c, x, top + ph, x, top + ph + 6, COL_AXIS)
        draw_line(c, x, top + 1, x, top + ph - 1, COL_GRID)
        draw_text(c, x - 12, top + ph + 16, str(x_pct), scale=2, color=COL_AXIS)

    for i in range(6):
        y_val = max_y * i / 5.0
        y = top + ph - int((y_val / max_y) * (ph - 1))
        draw_line(c, left - 5, y, left, y, COL_AXIS)
        draw_line(c, left + 1, y, left + pw, y, COL_GRID)
        draw_text(c, 20, y - 6, f"{y_val:.1f}", scale=2, color=COL_AXIS)

    for r in rows:
        x_d = left + int((r["d_hit_p10"] / 100.0) * pw)
        y_d = top + ph - int((r["speedup_p10"] / max_y) * (ph - 1))
        fill_circle(c, x_d, y_d, 4, COL_BLUE)

        x_i = left + int((r["i_hit_p10"] / 100.0) * pw)
        y_i = top + ph - int((r["speedup_p10"] / max_y) * (ph - 1))
        fill_circle(c, x_i, y_i, 3, COL_ORANGE)

    draw_text(c, left + (pw - text_width("X: CACHE HIT RATE (%)", 2)) // 2, top + ph + 56,
              "X: CACHE HIT RATE (%)", scale=2, color=COL_AXIS)
    draw_text(c, 12, top + ph // 2 - 10, "Y: SPEEDUP (X)", scale=2, color=COL_AXIS)

    lx, ly = left + pw - 210, top + 26
    fill_rect(c, lx, ly, lx + 14, ly + 14, COL_BLUE)
    draw_text(c, lx + 24, ly + 2, "D-HIT", scale=2)
    fill_rect(c, lx, ly + 24, lx + 14, ly + 38, COL_ORANGE)
    draw_text(c, lx + 24, ly + 26, "I-HIT", scale=2)

    write_png(path, c)


def render_bench_cycles(path, items):
    c = make_canvas(1200, 720)
    left, right, top, bottom = 130, 60, 100, 160
    pw = c["w"] - left - right
    ph = c["h"] - top - bottom

    draw_text(c, left, 24, "BENCHMARK CYCLES (LOG SCALE)", scale=4)
    draw_text(c, left, 52, "Y = LOG10(CYCLES)", scale=2, color=(50, 72, 98))

    draw_line(c, left, top + ph, left + pw, top + ph, COL_AXIS)
    draw_line(c, left, top, left, top + ph, COL_AXIS)

    logs = [math.log10(max(1, it["cycles"])) for it in items]
    mn = min(logs) if logs else 0.0
    mx = max(logs) if logs else 1.0
    span = max(1e-6, mx - mn)

    for i in range(6):
        lv = mn + (span * i / 5.0)
        y = top + ph - int(((lv - mn) / span) * (ph - 1))
        draw_line(c, left - 5, y, left, y, COL_AXIS)
        draw_line(c, left + 1, y, left + pw, y, COL_GRID)
        draw_text(c, 14, y - 6, f"{lv:.2f}", scale=2, color=COL_AXIS)

    n = max(1, len(items))
    step = pw / n
    bar_w = max(2, int(step * 0.68))
    palette = [COL_BLUE, COL_ORANGE, COL_GREEN, COL_PURPLE, (220, 70, 100), COL_GRAY]

    for i, it in enumerate(items):
        lv = math.log10(max(1, it["cycles"]))
        norm = (lv - mn) / span
        h = int(norm * (ph - 1))
        x0 = int(left + i * step + (step - bar_w) / 2)
        x1 = x0 + bar_w
        y0 = top + ph - h
        fill_rect(c, x0, y0, x1, top + ph - 1, palette[i % len(palette)])

        xc = left + int((i + 0.5) * step)
        draw_line(c, xc, top + ph, xc, top + ph + 6, COL_AXIS)
        draw_text(c, xc - 6, top + ph + 16, str(i + 1), scale=2, color=COL_AXIS)

    draw_text(c, left + (pw - text_width("X: BENCHMARK CASE INDEX", 2)) // 2, top + ph + 58,
              "X: BENCHMARK CASE INDEX", scale=2, color=COL_AXIS)
    draw_text(c, 12, top + ph // 2 - 10, "Y: LOG10(CYCLES)", scale=2, color=COL_AXIS)

    lx, ly = left + pw - 260, top + 16
    for i, it in enumerate(items):
        y = ly + i * 20
        fill_rect(c, lx, y, lx + 12, y + 12, palette[i % len(palette)])
        short_name = f"{i + 1}:{it['name']}".upper()
        draw_text(c, lx + 20, y + 2, short_name, scale=1)

    write_png(path, c)


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def parse_du_size_by_name(lines):
    out = {}
    for ln in lines:
        parts = re.split(r"\s+", ln.strip())
        if len(parts) < 2:
            continue
        size = parts[0]
        name = parts[-1]
        out[name] = size
    return out


def read_lines(path):
    if not path or not os.path.exists(path):
        return []
    with open(path, "r", encoding="utf-8") as f:
        return [ln.rstrip() for ln in f if ln.strip()]


def read_optional_csv_list(path):
    if not path or not os.path.exists(path):
        return []
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def read_optional_json(path):
    if not path or not os.path.exists(path):
        return None
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def classify_web_smoke(web_status_obj, web_health_obj, index_head):
    if isinstance(web_status_obj, dict):
        status = str(web_status_obj.get("status", "")).upper()
        reason = str(web_status_obj.get("reason", ""))
        if status in {"PASS", "WARN", "FAIL", "UNKNOWN"}:
            return ("PASS" if status == "PASS" else "WARN", reason)

    if not isinstance(web_health_obj, dict):
        return ("WARN", "web_health.json missing or invalid")

    if str(web_health_obj.get("status", "")).upper() in {"WARN", "FAIL", "UNKNOWN"}:
        return ("WARN", str(web_health_obj.get("reason", "health status not pass")))

    ok = web_health_obj.get("ok")
    index_ok = "doctype" in (index_head or "").lower()
    if ok is True and index_ok:
        return ("PASS", "sampled")
    if ok is True:
        return ("WARN", "health ok but index head does not contain doctype")
    return ("WARN", str(web_health_obj.get("reason", "health endpoint returned non-ok")))


def main():
    args = parse_args()
    paths = build_paths(
        args.run_tag,
        args.run_dir,
        args.cache_matrix_dir,
        args.gate_prefix,
        args.benchmark_dir,
        args.benchmark_gate_prefix,
    )

    ensure_dir(paths["fig_dir"])

    p1 = read_csv_rows(paths["p1_csv"])
    p10 = read_csv_rows(paths["p10_csv"])
    nc = read_csv_rows(paths["nc_csv"])

    common = sorted(set(p1.keys()) & set(p10.keys()) & set(nc.keys()))
    rows = []
    for t in common:
        r1 = p1[t]
        r10 = p10[t]
        rn = nc[t]
        c1 = to_int(r1.get("cycles"))
        c10 = to_int(r10.get("cycles"))
        cn = to_int(rn.get("cycles"))
        rows.append(
            {
                "test": t,
                "rc_p1": to_int(r1.get("rc")),
                "rc_p10": to_int(r10.get("rc")),
                "rc_nc": to_int(rn.get("rc")),
                "ms_p1": to_int(r1.get("ms")),
                "ms_p10": to_int(r10.get("ms")),
                "ms_nc": to_int(rn.get("ms")),
                "cycles_p1": c1,
                "cycles_p10": c10,
                "cycles_nc": cn,
                "instrs_p1": to_int(r1.get("instrs")),
                "instrs_p10": to_int(r10.get("instrs")),
                "instrs_nc": to_int(rn.get("instrs")),
                "i_hit_p10": to_float(r10.get("i_cache_hit_pct")),
                "d_hit_p10": to_float(r10.get("d_cache_hit_pct")),
                "stall_p10": to_int(r10.get("stall")),
                "cache_stall_p10": to_int(r10.get("cache_stall")),
                "hazard_stall_p10": to_int(r10.get("hazard_stall")),
                "i_cold_p10": to_int(r10.get("i_cold_miss")),
                "i_conflict_p10": to_int(r10.get("i_conflict_miss")),
                "i_capacity_p10": to_int(r10.get("i_capacity_miss")),
                "d_cold_p10": to_int(r10.get("d_cold_miss")),
                "d_conflict_p10": to_int(r10.get("d_conflict_miss")),
                "d_capacity_p10": to_int(r10.get("d_capacity_miss")),
                "speedup_p10": (cn / c10) if c10 > 0 else 0.0,
                "speedup_p1": (cn / c1) if c1 > 0 else 0.0,
                "penalty_ratio": (c10 / c1) if c1 > 0 else 0.0,
                "mem_test": is_mem_test(t),
            }
        )

    ctest = parse_ctest(paths["ctest_log"])

    benchmark_summary_rows = read_optional_csv_list(paths["benchmark_summary"])
    benchmark_detail_rows = read_optional_csv_list(paths["benchmark_detail"])
    benchmark_gate_result = read_optional_json(paths["benchmark_gate_result"])
    benchmark_matrix_rows = read_optional_csv_list(paths["benchmark_matrix_summary"])
    benchmark_cache_gate_result = read_optional_json(paths["benchmark_cache_gate_result"])

    benchmark_rows = []
    for row in benchmark_summary_rows:
        benchmark_rows.append(
            {
                "benchmark": row.get("benchmark", "-"),
                "rc_p1": to_int(row.get("rc_p1"), 127),
                "rc_p10": to_int(row.get("rc_p10"), 127),
                "rc_nocache": to_int(row.get("rc_nocache"), 127),
                "ms_p1": to_int(row.get("ms_p1")),
                "ms_p10": to_int(row.get("ms_p10")),
                "ms_nocache": to_int(row.get("ms_nocache")),
                "cycles_p1": to_int(row.get("cycles_p1")),
                "cycles_p10": to_int(row.get("cycles_p10")),
                "cycles_nocache": to_int(row.get("cycles_nocache")),
                "instrs_p1": to_int(row.get("instrs_p1")),
                "instrs_p10": to_int(row.get("instrs_p10")),
                "instrs_nocache": to_int(row.get("instrs_nocache")),
                "i_hit_p10": to_float(row.get("i_hit_p10")),
                "d_hit_p10": to_float(row.get("d_hit_p10")),
                "stall_p10": to_int(row.get("stall_p10")),
                "cache_stall_p10": to_int(row.get("cache_stall_p10")),
                "hazard_stall_p10": to_int(row.get("hazard_stall_p10")),
                "i_cold_miss_p10": to_int(row.get("i_cold_miss_p10")),
                "i_conflict_miss_p10": to_int(row.get("i_conflict_miss_p10")),
                "i_capacity_miss_p10": to_int(row.get("i_capacity_miss_p10")),
                "d_cold_miss_p10": to_int(row.get("d_cold_miss_p10")),
                "d_conflict_miss_p10": to_int(row.get("d_conflict_miss_p10")),
                "d_capacity_miss_p10": to_int(row.get("d_capacity_miss_p10")),
                "speedup_p10": to_float(row.get("speedup_p10")),
                "speedup_p1": to_float(row.get("speedup_p1")),
                "penalty_ratio": to_float(row.get("penalty_ratio_p10_over_p1")),
                "checksum_p10": row.get("checksum_p10", ""),
            }
        )
    benchmark_rows = sorted(benchmark_rows, key=lambda x: x["benchmark"])

    legacy_bench_items = []
    if not benchmark_rows:
        bench_rc = parse_bench_rcs(paths["bench_rc"])
        for name, p in paths["bench_logs"].items():
            d = parse_perf_line(p)
            d["name"] = name
            d["rc"] = bench_rc.get(name, -1)
            legacy_bench_items.append(d)

    bench_chart_items = []
    if benchmark_detail_rows:
        profile_order = {"p1": 0, "p10": 1, "nocache": 2}
        for row in sorted(
            benchmark_detail_rows,
            key=lambda x: (x.get("benchmark", ""), profile_order.get(x.get("profile", ""), 99)),
        ):
            bench_chart_items.append(
                {
                    "name": f"{row.get('benchmark', '-')}_{row.get('profile', '-')}",
                    "cycles": to_int(row.get("cycles")),
                }
            )
    elif benchmark_rows:
        profile_fields = [
            ("p1", "cycles_p1"),
            ("p10", "cycles_p10"),
            ("nocache", "cycles_nocache"),
        ]
        for br in benchmark_rows:
            for profile, field in profile_fields:
                bench_chart_items.append(
                    {
                        "name": f"{br['benchmark']}_{profile}",
                        "cycles": to_int(br.get(field)),
                    }
                )
    else:
        for bi in legacy_bench_items:
            bench_chart_items.append({"name": bi["name"], "cycles": bi["cycles"]})

    matrix_rows = read_optional_csv_list(paths["matrix_summary"])
    gate_result = read_optional_json(paths["gate_result"])

    speedups = [r["speedup_p10"] for r in rows if r["speedup_p10"] > 0]
    penalty_ratios = [r["penalty_ratio"] for r in rows if r["penalty_ratio"] > 0]
    d_hits = [r["d_hit_p10"] for r in rows]
    i_hits = [r["i_hit_p10"] for r in rows]

    mem_rows = [r for r in rows if r["mem_test"]]
    non_mem_rows = [r for r in rows if not r["mem_test"]]

    stats = {
        "count": len(rows),
        "p1_pass": sum(1 for r in rows if r["rc_p1"] == 0),
        "p10_pass": sum(1 for r in rows if r["rc_p10"] == 0),
        "nc_pass": sum(1 for r in rows if r["rc_nc"] == 0),
        "avg_speedup_p10": statistics.mean(speedups) if speedups else 0.0,
        "median_speedup_p10": statistics.median(speedups) if speedups else 0.0,
        "p90_speedup_p10": percentile(speedups, 0.9),
        "geomean_speedup_p10": geomean(speedups),
        "avg_penalty_ratio": statistics.mean(penalty_ratios) if penalty_ratios else 0.0,
        "avg_ms_p1": statistics.mean([r["ms_p1"] for r in rows]) if rows else 0.0,
        "avg_ms_p10": statistics.mean([r["ms_p10"] for r in rows]) if rows else 0.0,
        "avg_ms_nc": statistics.mean([r["ms_nc"] for r in rows]) if rows else 0.0,
        "corr_d_speedup": corr(d_hits, speedups) if rows else 0.0,
        "corr_i_speedup": corr(i_hits, speedups) if rows else 0.0,
        "mem_avg_speedup": statistics.mean([r["speedup_p10"] for r in mem_rows]) if mem_rows else 0.0,
        "non_mem_avg_speedup": statistics.mean([r["speedup_p10"] for r in non_mem_rows]) if non_mem_rows else 0.0,
        "avg_stall_p10": statistics.mean([r["stall_p10"] for r in rows]) if rows else 0.0,
        "avg_cache_stall_p10": statistics.mean([r["cache_stall_p10"] for r in rows]) if rows else 0.0,
        "avg_hazard_stall_p10": statistics.mean([r["hazard_stall_p10"] for r in rows]) if rows else 0.0,
        "avg_i_cold_p10": statistics.mean([r["i_cold_p10"] for r in rows]) if rows else 0.0,
        "avg_i_conflict_p10": statistics.mean([r["i_conflict_p10"] for r in rows]) if rows else 0.0,
        "avg_i_capacity_p10": statistics.mean([r["i_capacity_p10"] for r in rows]) if rows else 0.0,
        "avg_d_cold_p10": statistics.mean([r["d_cold_p10"] for r in rows]) if rows else 0.0,
        "avg_d_conflict_p10": statistics.mean([r["d_conflict_p10"] for r in rows]) if rows else 0.0,
        "avg_d_capacity_p10": statistics.mean([r["d_capacity_p10"] for r in rows]) if rows else 0.0,
    }

    total_i_miss = sum(r["i_cold_p10"] + r["i_conflict_p10"] + r["i_capacity_p10"] for r in rows)
    total_d_miss = sum(r["d_cold_p10"] + r["d_conflict_p10"] + r["d_capacity_p10"] for r in rows)
    stats["i_cold_share"] = (sum(r["i_cold_p10"] for r in rows) / total_i_miss) if total_i_miss else 0.0
    stats["i_conflict_share"] = (sum(r["i_conflict_p10"] for r in rows) / total_i_miss) if total_i_miss else 0.0
    stats["i_capacity_share"] = (sum(r["i_capacity_p10"] for r in rows) / total_i_miss) if total_i_miss else 0.0
    stats["d_cold_share"] = (sum(r["d_cold_p10"] for r in rows) / total_d_miss) if total_d_miss else 0.0
    stats["d_conflict_share"] = (sum(r["d_conflict_p10"] for r in rows) / total_d_miss) if total_d_miss else 0.0
    stats["d_capacity_share"] = (sum(r["d_capacity_p10"] for r in rows) / total_d_miss) if total_d_miss else 0.0

    rows_sorted = sorted(rows, key=lambda x: x["speedup_p10"], reverse=True)
    top5 = rows_sorted[:5]
    bot5 = sorted(rows, key=lambda x: x["speedup_p10"])[:5]

    render_speedup_bar(paths["speedup_fig"], rows_sorted)
    render_scatter(paths["scatter_fig"], rows)
    render_bench_cycles(paths["bench_fig"], bench_chart_items)

    with open(paths["summary_csv"], "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "test",
            "rc_p1",
            "rc_p10",
            "rc_nocache",
            "ms_p1",
            "ms_p10",
            "ms_nocache",
            "cycles_p1",
            "cycles_p10",
            "cycles_nocache",
            "speedup_p10",
            "speedup_p1",
            "penalty_ratio_p10_over_p1",
            "i_hit_p10",
            "d_hit_p10",
            "stall_p10",
            "cache_stall_p10",
            "hazard_stall_p10",
            "i_cold_miss_p10",
            "i_conflict_miss_p10",
            "i_capacity_miss_p10",
            "d_cold_miss_p10",
            "d_conflict_miss_p10",
            "d_capacity_miss_p10",
            "mem_test",
        ])
        for r in rows:
            writer.writerow([
                r["test"],
                r["rc_p1"],
                r["rc_p10"],
                r["rc_nc"],
                r["ms_p1"],
                r["ms_p10"],
                r["ms_nc"],
                r["cycles_p1"],
                r["cycles_p10"],
                r["cycles_nc"],
                "{:.6f}".format(r["speedup_p10"]),
                "{:.6f}".format(r["speedup_p1"]),
                "{:.6f}".format(r["penalty_ratio"]),
                "{:.2f}".format(r["i_hit_p10"]),
                "{:.2f}".format(r["d_hit_p10"]),
                r["stall_p10"],
                r["cache_stall_p10"],
                r["hazard_stall_p10"],
                r["i_cold_p10"],
                r["i_conflict_p10"],
                r["i_capacity_p10"],
                r["d_cold_p10"],
                r["d_conflict_p10"],
                r["d_capacity_p10"],
                1 if r["mem_test"] else 0,
            ])

    disk_core_lines = read_lines(paths["disk_core"])
    disk_top_lines = read_lines(paths["disk_top"])

    web_health = "{\"status\":\"UNKNOWN\",\"reason\":\"web_health.json missing\"}"
    web_health_obj = None
    if paths["web_health"] and os.path.exists(paths["web_health"]):
        with open(paths["web_health"], "r", encoding="utf-8") as f:
            web_health = f.read().strip()
        try:
            web_health_obj = json.loads(web_health)
        except Exception:
            web_health_obj = None

    web_index_head_lines = read_lines(paths["web_index_head"])
    web_index_head = web_index_head_lines[0] if web_index_head_lines else "<missing>"
    web_smoke_status_obj = read_optional_json(paths["web_smoke_status"])
    web_smoke_status, web_smoke_reason = classify_web_smoke(web_smoke_status_obj, web_health_obj, web_index_head)

    du_core = parse_du_size_by_name(disk_core_lines)
    build_size = du_core.get("build", "-")
    tmp_size = du_core.get("tmp", "-")
    testing_size = du_core.get("Testing", "-")

    run_dir_rel = os.path.relpath(paths["run_dir"], ROOT).replace("\\", "/")
    benchmark_dir_rel = ""
    if paths["benchmark_dir"]:
        benchmark_dir_rel = os.path.relpath(paths["benchmark_dir"], ROOT).replace("\\", "/")
    matrix_dir_rel = ""
    if paths["matrix_dir"]:
        matrix_dir_rel = os.path.relpath(paths["matrix_dir"], ROOT).replace("\\", "/")

    lines = []
    lines.append("# FULL TEST PERFORMANCE REPORT")
    lines.append("")
    lines.append("生成日期：{}".format(args.report_date))
    lines.append("")
    lines.append("## 0. 数据来源与口径定义")
    lines.append("")
    lines.append("### 0.1 输入数据文件")
    lines.append("")
    lines.append("| 数据类别 | 输入文件 | 处理用途 |")
    lines.append("|---|---|---|")
    lines.append("| rv32ui p1 | `docs/rv32ui_perf_full_p1.csv` | cache on + penalty=1 的基线 |")
    lines.append("| rv32ui p10 | `docs/rv32ui_perf_full_p10.csv` | cache on + penalty=10 的对比组 |")
    lines.append("| rv32ui no-cache | `docs/rv32ui_perf_full_nocache.csv` | cache off 的基线组 |")
    lines.append("| ctest 日志 | `{}/ctest_full.log` | 正确性统计（ctest 全量） |".format(run_dir_rel))
    if benchmark_rows:
        lines.append("| benchmark 汇总 | `{}/benchmark_summary.csv` | benchmark 三配置（p1/p10/no-cache）汇总统计 |".format(benchmark_dir_rel))
        lines.append("| benchmark 明细 | `{}/benchmark_detail.csv` | benchmark profile 级明细（用于图表） |".format(benchmark_dir_rel))
        lines.append("| benchmark 原始 CSV | `{}/benchmark_p1.csv` / `{}/benchmark_p10.csv` / `{}/benchmark_nocache.csv` | 每个 profile 的原始输出 |".format(benchmark_dir_rel, benchmark_dir_rel, benchmark_dir_rel))
    else:
        lines.append("| benchmark 返回码 | `{}/benchmark_rcs.csv` | 组合场景正确性检查 |".format(run_dir_rel))
        lines.append("| benchmark 性能日志 | `{}/{{hello,matmul,quicksort}}_*.log` | cycles/instrs/hit/stall 提取 |".format(run_dir_rel))
    if benchmark_matrix_rows and matrix_dir_rel:
        lines.append("| benchmark cache matrix 汇总 | `{}/benchmark_policy_summary.csv` | benchmark 在 5 策略下的汇总统计 |".format(matrix_dir_rel))
        lines.append("| benchmark cache matrix 明细 | `{}/benchmark_matrix_detail.csv` | benchmark x cache 策略明细 |".format(matrix_dir_rel))
    lines.append("")
    lines.append("### 0.2 三组配置的含义")
    lines.append("")
    lines.append("| 配置名 | cache 状态 | miss penalty | 含义 |")
    lines.append("|---|---|---:|---|")
    lines.append("| p1 | 开启 | 1 | 低 miss 代价组，用于观察理想 cache 效果 |")
    lines.append("| p10 | 开启 | 10 | 高 miss 代价组，用于观察 miss 对性能放大效应 |")
    lines.append("| no-cache | 关闭 | 10 | 无 cache 基线；访存直接走内存路径 |")
    lines.append("")
    lines.append("### 0.3 指标定义")
    lines.append("")
    lines.append("- `speedup_p10 = cycles_nocache / cycles_p10`。值越大表示 cache 收益越高。")
    lines.append("- `speedup_p1 = cycles_nocache / cycles_p1`。用于评估低 penalty 下收益上限。")
    lines.append("- `penalty_ratio = cycles_p10 / cycles_p1`。用于评估 workload 对 miss penalty 敏感度。")
    lines.append("- 相关系数 `corr(D-hit, speedup)` 与 `corr(I-hit, speedup)` 用于衡量命中率与收益关系。")
    lines.append("")
    lines.append("### 0.4 数据处理流程")
    lines.append("")
    lines.append("1. 按 test 名称对 p1/p10/no-cache 三份 CSV 做交集对齐。")
    lines.append("2. 逐项计算 speedup/penalty ratio，并按访存类与非访存类分组。")
    if benchmark_rows:
        lines.append("3. 从 benchmark_summary/detail.csv 读取 cycles/hit/stall 与 speedup 指标，形成工作负载级对比。")
    else:
        lines.append("3. 从 benchmark 日志提取 cycles/instrs/hit/stall 指标，形成工作负载级对比。")
    if benchmark_matrix_rows:
        lines.append("4. 从 benchmark_policy_summary.csv 读取 cache 策略矩阵，做跨策略回归分析。")
    lines.append("5. 生成汇总 CSV、三张 PNG 图和本 Markdown 报告。")
    lines.append("")
    lines.append("## 1. 执行范围")
    lines.append("")
    if ctest["total"] > 0:
        lines.append("- ctest 全量（{} 项）".format(ctest["total"]))
    else:
        lines.append("- ctest 全量（以当前 build 目录为准）")
    lines.append("- rv32ui 全量（42 项）x 3 组配置：p1 / p10 / no-cache")
    lines.append("- benchmark 组合：hello、matmul（cache/no-cache）、quicksort（cache/no-cache/write-through）")
    if benchmark_matrix_rows:
        lines.append("- benchmark cache matrix：wb_wa / wb_nowa / wt_wa / wt_nowa / nocache")
    lines.append("- Web smoke：trace_server 健康检查与首页可达")
    lines.append("")
    lines.append("## 2. 正确性结果")
    lines.append("")
    lines.append("- ctest: {}/{} 通过，失败 {}。".format(
        ctest["total"] - ctest["failed"], ctest["total"], ctest["failed"]
    ))
    lines.append("- rv32ui p1: {}/{} 通过。".format(stats["p1_pass"], stats["count"]))
    lines.append("- rv32ui p10: {}/{} 通过。".format(stats["p10_pass"], stats["count"]))
    lines.append("- rv32ui no-cache: {}/{} 通过。".format(stats["nc_pass"], stats["count"]))
    lines.append("- benchmark 返回码：")
    lines.append("")
    if benchmark_rows:
        lines.append("| benchmark | rc_p1 | rc_p10 | rc_nocache |")
        lines.append("|---|---:|---:|---:|")
        for br in benchmark_rows:
            lines.append(
                "| {} | {} | {} | {} |".format(
                    br["benchmark"], br["rc_p1"], br["rc_p10"], br["rc_nocache"]
                )
            )
    else:
        lines.append("| case | rc |")
        lines.append("|---|---:|")
        for bi in legacy_bench_items:
            lines.append("| {} | {} |".format(bi["name"], bi["rc"]))
    lines.append("")
    lines.append("## 3. 性能统计（rv32ui）")
    lines.append("")
    lines.append("- p10 平均 speedup: {:.2f}x".format(stats["avg_speedup_p10"]))
    lines.append("- p10 中位数 speedup: {:.2f}x".format(stats["median_speedup_p10"]))
    lines.append("- p10 P90 speedup: {:.2f}x".format(stats["p90_speedup_p10"]))
    lines.append("- p10 几何均值 speedup: {:.2f}x".format(stats["geomean_speedup_p10"]))
    lines.append("- p10/p1 平均 cycle 比: {:.2f}x".format(stats["avg_penalty_ratio"]))
    lines.append("- 平均执行时长 ms（p1 / p10 / no-cache）: {:.2f} / {:.2f} / {:.2f}".format(
        stats["avg_ms_p1"], stats["avg_ms_p10"], stats["avg_ms_nc"]
    ))
    lines.append("- 访存密集测试平均 speedup: {:.2f}x；非访存测试平均 speedup: {:.2f}x".format(
        stats["mem_avg_speedup"], stats["non_mem_avg_speedup"]
    ))
    lines.append("- D-hit 与 speedup 相关系数: {:.3f}".format(stats["corr_d_speedup"]))
    lines.append("- I-hit 与 speedup 相关系数: {:.3f}".format(stats["corr_i_speedup"]))
    lines.append("")
    lines.append("### Cache Stall 与 Miss 分解（p10）")
    lines.append("")
    lines.append(
        "- 平均 stall 拆分（stall / cache_stall / hazard_stall）: {:.2f} / {:.2f} / {:.2f}".format(
            stats["avg_stall_p10"], stats["avg_cache_stall_p10"], stats["avg_hazard_stall_p10"]
        )
    )
    lines.append(
        "- 平均 I-miss 分解（cold/conflict/capacity）: {:.2f} / {:.2f} / {:.2f}".format(
            stats["avg_i_cold_p10"], stats["avg_i_conflict_p10"], stats["avg_i_capacity_p10"]
        )
    )
    lines.append(
        "- 平均 D-miss 分解（cold/conflict/capacity）: {:.2f} / {:.2f} / {:.2f}".format(
            stats["avg_d_cold_p10"], stats["avg_d_conflict_p10"], stats["avg_d_capacity_p10"]
        )
    )
    lines.append(
        "- I-miss 占比（cold/conflict/capacity）: {:.1f}% / {:.1f}% / {:.1f}%".format(
            stats["i_cold_share"] * 100.0,
            stats["i_conflict_share"] * 100.0,
            stats["i_capacity_share"] * 100.0,
        )
    )
    lines.append(
        "- D-miss 占比（cold/conflict/capacity）: {:.1f}% / {:.1f}% / {:.1f}%".format(
            stats["d_cold_share"] * 100.0,
            stats["d_conflict_share"] * 100.0,
            stats["d_capacity_share"] * 100.0,
        )
    )

    top_d_conflict = sorted(rows, key=lambda x: x["d_conflict_p10"], reverse=True)[:5]
    if top_d_conflict and top_d_conflict[0]["d_conflict_p10"] > 0:
        lines.append("")
        lines.append("#### D-conflict miss Top 5")
        lines.append("")
        lines.append("| test | d_conflict_miss | d_capacity_miss | d_hit |")
        lines.append("|---|---:|---:|---:|")
        for r in top_d_conflict:
            lines.append(
                "| {} | {} | {} | {:.2f}% |".format(
                    r["test"], r["d_conflict_p10"], r["d_capacity_p10"], r["d_hit_p10"]
                )
            )
    else:
        lines.append("- miss 分解字段当前为 0；若需专项分析，请先使用新版 `test_all.sh` 重跑 CSV。")

    lines.append("")
    lines.append("### Cache 回归矩阵与门禁")
    lines.append("")
    if matrix_rows:
        lines.append("| policy | pass/tests | avg_cycles | avg_i_hit_pct | avg_d_hit_pct | avg_speedup_vs_nocache |")
        lines.append("|---|---:|---:|---:|---:|---:|")
        for mr in matrix_rows:
            lines.append(
                "| {} | {}/{} | {} | {} | {} | {} |".format(
                    mr.get("policy", "-"),
                    mr.get("pass", "0"),
                    mr.get("tests", "0"),
                    mr.get("avg_cycles", "0"),
                    mr.get("avg_i_hit_pct", "0"),
                    mr.get("avg_d_hit_pct", "0"),
                    mr.get("avg_speedup_vs_nocache", "0"),
                )
            )
        lines.append("")
        lines.append("- matrix summary: `{}`".format(os.path.relpath(paths["matrix_summary"], ROOT).replace("\\", "/")))
        lines.append("- matrix detail: `{}`".format(os.path.relpath(paths["matrix_detail"], ROOT).replace("\\", "/")))
    else:
        lines.append("- 未发现 `policy_summary.csv`；可先运行 `tools/run_cache_matrix.sh` 生成矩阵数据。")

    if gate_result:
        lines.append("")
        lines.append("- gate status: **{}** (baseline: `{}`)".format(gate_result.get("status", "UNKNOWN"), gate_result.get("baseline", "-")))
        issues = gate_result.get("issues", [])
        lines.append("- gate issues: {}".format(len(issues)))
        if issues:
            lines.append("")
            lines.append("| severity | policy | metric | value | threshold | message |")
            lines.append("|---|---|---|---:|---:|---|")
            for it in issues[:10]:
                lines.append(
                    "| {} | {} | {} | {:.4f} | {:.4f} | {} |".format(
                        it.get("severity", "-"),
                        it.get("policy", "-"),
                        it.get("metric", "-"),
                        to_float(it.get("value")),
                        to_float(it.get("threshold")),
                        it.get("message", "-"),
                    )
                )
        if paths["gate_report"] and os.path.exists(paths["gate_report"]):
            lines.append("- gate report: `{}`".format(os.path.relpath(paths["gate_report"], ROOT).replace("\\", "/")))
    else:
        lines.append("- 未发现 gate 结果；可执行 `python3 tools/check_cache_gate.py --summary <policy_summary.csv>` 生成门禁结论。")

    lines.append("")
    lines.append("### Benchmark Cache Matrix 与门禁")
    lines.append("")
    if benchmark_matrix_rows:
        lines.append("| policy | pass/benchmarks | avg_cycles | avg_i_hit_pct | avg_d_hit_pct | avg_speedup_vs_nocache |")
        lines.append("|---|---:|---:|---:|---:|---:|")
        for row in benchmark_matrix_rows:
            lines.append(
                "| {} | {}/{} | {} | {} | {} | {} |".format(
                    row.get("policy", "-"),
                    row.get("pass", "0"),
                    row.get("benchmarks", row.get("tests", "0")),
                    row.get("avg_cycles", "0"),
                    row.get("avg_i_hit_pct", "0"),
                    row.get("avg_d_hit_pct", "0"),
                    row.get("avg_speedup_vs_nocache", "0"),
                )
            )

        lines.append("")
        lines.append("- benchmark matrix summary: `{}`".format(os.path.relpath(paths["benchmark_matrix_summary"], ROOT).replace("\\", "/")))
        if paths["benchmark_matrix_detail"] and os.path.exists(paths["benchmark_matrix_detail"]):
            lines.append("- benchmark matrix detail: `{}`".format(os.path.relpath(paths["benchmark_matrix_detail"], ROOT).replace("\\", "/")))
    else:
        lines.append("- 未发现 benchmark cache matrix 汇总；可执行 `tools/run_benchmark_cache_matrix.sh` 生成。")

    if benchmark_cache_gate_result:
        lines.append("- benchmark cache gate status: **{}**".format(benchmark_cache_gate_result.get("status", "UNKNOWN")))
        bench_cache_issues = benchmark_cache_gate_result.get("issues", [])
        lines.append("- benchmark cache gate issues: {}".format(len(bench_cache_issues)))
        if bench_cache_issues:
            lines.append("")
            lines.append("| severity | policy | metric | value | threshold | message |")
            lines.append("|---|---|---|---:|---:|---|")
            for it in bench_cache_issues[:10]:
                lines.append(
                    "| {} | {} | {} | {:.4f} | {:.4f} | {} |".format(
                        it.get("severity", "-"),
                        it.get("policy", "-"),
                        it.get("metric", "-"),
                        to_float(it.get("value")),
                        to_float(it.get("threshold")),
                        it.get("message", "-"),
                    )
                )
        if paths["benchmark_cache_gate_report"] and os.path.exists(paths["benchmark_cache_gate_report"]):
            lines.append("- benchmark cache gate report: `{}`".format(os.path.relpath(paths["benchmark_cache_gate_report"], ROOT).replace("\\", "/")))
    else:
        lines.append("- 未发现 benchmark cache gate 结果；可执行 `python3 tools/check_benchmark_cache_gate.py --summary <benchmark_policy_summary.csv>` 生成。")

    lines.append("")
    lines.append("### Top 5 speedup（p10）")
    lines.append("")
    lines.append("| test | speedup | cycles_nocache | cycles_p10 |")
    lines.append("|---|---:|---:|---:|")
    for r in top5:
        lines.append("| {} | {:.2f}x | {} | {} |".format(r["test"], r["speedup_p10"], r["cycles_nc"], r["cycles_p10"]))
    lines.append("")
    lines.append("### Bottom 5 speedup（p10）")
    lines.append("")
    lines.append("| test | speedup | cycles_nocache | cycles_p10 |")
    lines.append("|---|---:|---:|---:|")
    for r in bot5:
        lines.append("| {} | {:.2f}x | {} | {} |".format(r["test"], r["speedup_p10"], r["cycles_nc"], r["cycles_p10"]))
    lines.append("")
    lines.append("## 4. Benchmark 观察")
    lines.append("")
    if benchmark_rows:
        lines.append("| benchmark | cycles_p1 | cycles_p10 | cycles_nocache | speedup_p10 | speedup_p1 | penalty_ratio | i_hit_p10 | d_hit_p10 | stall_p10 | checksum_p10 |")
        lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|")
        for br in benchmark_rows:
            lines.append(
                "| {} | {} | {} | {} | {:.2f}x | {:.2f}x | {:.2f}x | {:.2f}% | {:.2f}% | {} | {} |".format(
                    br["benchmark"],
                    br["cycles_p1"],
                    br["cycles_p10"],
                    br["cycles_nocache"],
                    br["speedup_p10"],
                    br["speedup_p1"],
                    br["penalty_ratio"],
                    br["i_hit_p10"],
                    br["d_hit_p10"],
                    br["stall_p10"],
                    br["checksum_p10"] if br["checksum_p10"] else "-",
                )
            )

        bench_speedups = [b["speedup_p10"] for b in benchmark_rows if b["speedup_p10"] > 0]
        bench_penalty = [b["penalty_ratio"] for b in benchmark_rows if b["penalty_ratio"] > 0]
        lines.append("")
        if bench_speedups:
            lines.append("- benchmark 平均 speedup_p10: {:.2f}x；中位数: {:.2f}x。".format(
                statistics.mean(bench_speedups), statistics.median(bench_speedups)
            ))
        if bench_penalty:
            lines.append("- benchmark 平均 penalty_ratio_p10_over_p1: {:.2f}x。".format(statistics.mean(bench_penalty)))

        bench_by_name = {b["benchmark"]: b for b in benchmark_rows}
        matmul = bench_by_name.get("matmul")
        qsort = bench_by_name.get("quicksort_stress") or bench_by_name.get("quicksort")
        if matmul and matmul["cycles_p10"] > 0:
            lines.append("- matmul（no-cache / p10）cycle 比: {:.2f}x".format(matmul["cycles_nocache"] / matmul["cycles_p10"]))
        if qsort and qsort["cycles_p10"] > 0:
            lines.append("- quicksort_stress（no-cache / p10）cycle 比: {:.2f}x".format(qsort["cycles_nocache"] / qsort["cycles_p10"]))

        if benchmark_gate_result:
            lines.append("- benchmark gate status: **{}**".format(benchmark_gate_result.get("status", "UNKNOWN")))
            bench_gate_issues = benchmark_gate_result.get("issues", [])
            lines.append("- benchmark gate issues: {}".format(len(bench_gate_issues)))
            if bench_gate_issues:
                lines.append("")
                lines.append("| severity | benchmark | metric | value | threshold | message |")
                lines.append("|---|---|---|---:|---:|---|")
                for it in bench_gate_issues[:10]:
                    lines.append(
                        "| {} | {} | {} | {:.4f} | {:.4f} | {} |".format(
                            it.get("severity", "-"),
                            it.get("benchmark", "-"),
                            it.get("metric", "-"),
                            to_float(it.get("value")),
                            to_float(it.get("threshold")),
                            it.get("message", "-"),
                        )
                    )
        if paths["benchmark_summary"] and os.path.exists(paths["benchmark_summary"]):
            lines.append("- benchmark summary: `{}`".format(os.path.relpath(paths["benchmark_summary"], ROOT).replace("\\", "/")))
        if paths["benchmark_detail"] and os.path.exists(paths["benchmark_detail"]):
            lines.append("- benchmark detail: `{}`".format(os.path.relpath(paths["benchmark_detail"], ROOT).replace("\\", "/")))
        if paths["benchmark_gate_report"] and os.path.exists(paths["benchmark_gate_report"]):
            lines.append("- benchmark gate report: `{}`".format(os.path.relpath(paths["benchmark_gate_report"], ROOT).replace("\\", "/")))
        lines.append("")
    else:
        lines.append("| case | cycles | instrs | i_hit | d_hit | stall | cache_stall | hazard_stall | checksum |")
        lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---|")
        for bi in legacy_bench_items:
            lines.append(
                "| {} | {} | {} | {:.2f}% | {:.2f}% | {} | {} | {} | {} |".format(
                    bi["name"],
                    bi["cycles"],
                    bi["instrs"],
                    bi["i_hit"],
                    bi["d_hit"],
                    bi["stall"],
                    bi["cache_stall"],
                    bi["hazard_stall"],
                    bi["checksum"] if bi["checksum"] else "-",
                )
            )
        lines.append("")

        matmul_cache = next((b for b in legacy_bench_items if b["name"] == "matmul_cache_p10"), None)
        matmul_nc = next((b for b in legacy_bench_items if b["name"] == "matmul_nocache_p10"), None)
        qsort_cache = next((b for b in legacy_bench_items if b["name"] == "quicksort_cache_default"), None)
        qsort_nc = next((b for b in legacy_bench_items if b["name"] == "quicksort_nocache"), None)
        if matmul_cache and matmul_nc and matmul_cache["cycles"] > 0:
            lines.append("- matmul（no-cache / cache）cycle 比: {:.2f}x".format(matmul_nc["cycles"] / matmul_cache["cycles"]))
        if qsort_cache and qsort_nc and qsort_cache["cycles"] > 0:
            lines.append("- quicksort（no-cache / cache）cycle 比: {:.2f}x".format(qsort_nc["cycles"] / qsort_cache["cycles"]))
        lines.append("")

    speedup_name = os.path.basename(paths["speedup_fig"])
    scatter_name = os.path.basename(paths["scatter_fig"])
    bench_name = os.path.basename(paths["bench_fig"])

    lines.append("## 5. 图表与说明")
    lines.append("")
    lines.append("图1：rv32ui speedup 条形图")
    lines.append("")
    lines.append(f"![speedup](figures/{speedup_name})")
    lines.append("")
    lines.append("- 标题：RV32UI SPEEDUP DISTRIBUTION。")
    lines.append("- X轴：测试索引（按 speedup 从高到低排序）。")
    lines.append("- Y轴：speedup 倍数（no-cache cycles / p10 cycles）。")
    lines.append("- 图例：蓝=非访存测试、橙=访存测试、红线=平均 speedup。")
    lines.append("- 说明：该图用于识别 cache 收益分布和尾部低收益用例。")
    lines.append("")
    lines.append("图2：命中率与 speedup 散点图")
    lines.append("")
    lines.append(f"![hitrate_scatter](figures/{scatter_name})")
    lines.append("")
    lines.append("- 标题：CACHE HIT RATE VS SPEEDUP。")
    lines.append("- X轴：cache hit rate (%)。")
    lines.append("- Y轴：speedup 倍数。")
    lines.append("- 图例：蓝点=D-hit，橙点=I-hit。")
    lines.append("- 说明：用于观察命中率提升与性能收益的相关关系。")
    lines.append("")
    lines.append("图3：benchmark cycles 对比（对数）")
    lines.append("")
    lines.append(f"![benchmark_cycles](figures/{bench_name})")
    lines.append("")
    lines.append("- 标题：BENCHMARK CYCLES (LOG SCALE)。")
    lines.append("- X轴：benchmark case 索引。")
    lines.append("- Y轴：log10(cycles)。")
    lines.append("- 图例：不同颜色对应不同 benchmark case。")
    lines.append("- 说明：对数坐标可在同图中比较百万级与百级 workload。")
    lines.append("")

    lines.append("## 6. Web smoke")
    lines.append("")
    lines.append("- 采样状态：**{}**".format(web_smoke_status))
    lines.append("- 状态说明：{}".format(web_smoke_reason if web_smoke_reason else "-"))
    lines.append("- 健康检查响应：")
    lines.append("")
    lines.append("{}".format(web_health))
    lines.append("")
    lines.append("- 首页首行：{}".format(web_index_head))
    if paths["web_health"] and os.path.exists(paths["web_health"]):
        lines.append("- web_health 文件：`{}`".format(os.path.relpath(paths["web_health"], ROOT).replace("\\", "/")))
    if paths["web_index_head"] and os.path.exists(paths["web_index_head"]):
        lines.append("- web_index_head 文件：`{}`".format(os.path.relpath(paths["web_index_head"], ROOT).replace("\\", "/")))
    if paths["web_smoke_status"] and os.path.exists(paths["web_smoke_status"]):
        lines.append("- web_smoke_status 文件：`{}`".format(os.path.relpath(paths["web_smoke_status"], ROOT).replace("\\", "/")))
    lines.append("")

    lines.append("## 7. 磁盘占用（清理前）")
    lines.append("")
    lines.append("### 核心目录占用")
    lines.append("")
    for ln in disk_core_lines:
        lines.append("- {}".format(ln))
    lines.append("")
    lines.append("### 根目录 Top10")
    lines.append("")
    for ln in disk_top_lines:
        lines.append("- {}".format(ln))
    lines.append("")

    lines.append("## 8. 产物索引")
    lines.append("")
    lines.append("- [docs/rv32ui_perf_full_p1.csv](rv32ui_perf_full_p1.csv)")
    lines.append("- [docs/rv32ui_perf_full_p10.csv](rv32ui_perf_full_p10.csv)")
    lines.append("- [docs/rv32ui_perf_full_nocache.csv](rv32ui_perf_full_nocache.csv)")
    lines.append("- [docs/{}]({})".format(os.path.basename(paths["summary_csv"]), os.path.basename(paths["summary_csv"])))
    lines.append("- [docs/figures/{}](figures/{})".format(speedup_name, speedup_name))
    lines.append("- [docs/figures/{}](figures/{})".format(scatter_name, scatter_name))
    lines.append("- [docs/figures/{}](figures/{})".format(bench_name, bench_name))
    if paths["matrix_summary"] and os.path.exists(paths["matrix_summary"]):
        matrix_summary_rel = os.path.relpath(paths["matrix_summary"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(matrix_summary_rel, matrix_summary_rel))
    if paths["matrix_detail"] and os.path.exists(paths["matrix_detail"]):
        matrix_detail_rel = os.path.relpath(paths["matrix_detail"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(matrix_detail_rel, matrix_detail_rel))
    if paths["benchmark_matrix_summary"] and os.path.exists(paths["benchmark_matrix_summary"]):
        benchmark_matrix_summary_rel = os.path.relpath(paths["benchmark_matrix_summary"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_matrix_summary_rel, benchmark_matrix_summary_rel))
    if paths["benchmark_matrix_detail"] and os.path.exists(paths["benchmark_matrix_detail"]):
        benchmark_matrix_detail_rel = os.path.relpath(paths["benchmark_matrix_detail"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_matrix_detail_rel, benchmark_matrix_detail_rel))
    if paths["gate_checks"] and os.path.exists(paths["gate_checks"]):
        gate_checks_rel = os.path.relpath(paths["gate_checks"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(gate_checks_rel, gate_checks_rel))
    if paths["gate_result"] and os.path.exists(paths["gate_result"]):
        gate_result_rel = os.path.relpath(paths["gate_result"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(gate_result_rel, gate_result_rel))
    if paths["gate_report"] and os.path.exists(paths["gate_report"]):
        gate_report_rel = os.path.relpath(paths["gate_report"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(gate_report_rel, gate_report_rel))
    if paths["benchmark_cache_gate_checks"] and os.path.exists(paths["benchmark_cache_gate_checks"]):
        benchmark_cache_gate_checks_rel = os.path.relpath(paths["benchmark_cache_gate_checks"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_cache_gate_checks_rel, benchmark_cache_gate_checks_rel))
    if paths["benchmark_cache_gate_result"] and os.path.exists(paths["benchmark_cache_gate_result"]):
        benchmark_cache_gate_result_rel = os.path.relpath(paths["benchmark_cache_gate_result"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_cache_gate_result_rel, benchmark_cache_gate_result_rel))
    if paths["benchmark_cache_gate_report"] and os.path.exists(paths["benchmark_cache_gate_report"]):
        benchmark_cache_gate_report_rel = os.path.relpath(paths["benchmark_cache_gate_report"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_cache_gate_report_rel, benchmark_cache_gate_report_rel))
    if paths["benchmark_p1"] and os.path.exists(paths["benchmark_p1"]):
        benchmark_p1_rel = os.path.relpath(paths["benchmark_p1"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_p1_rel, benchmark_p1_rel))
    if paths["benchmark_p10"] and os.path.exists(paths["benchmark_p10"]):
        benchmark_p10_rel = os.path.relpath(paths["benchmark_p10"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_p10_rel, benchmark_p10_rel))
    if paths["benchmark_nocache"] and os.path.exists(paths["benchmark_nocache"]):
        benchmark_nc_rel = os.path.relpath(paths["benchmark_nocache"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_nc_rel, benchmark_nc_rel))
    if paths["benchmark_detail"] and os.path.exists(paths["benchmark_detail"]):
        benchmark_detail_rel = os.path.relpath(paths["benchmark_detail"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_detail_rel, benchmark_detail_rel))
    if paths["benchmark_summary"] and os.path.exists(paths["benchmark_summary"]):
        benchmark_summary_rel = os.path.relpath(paths["benchmark_summary"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_summary_rel, benchmark_summary_rel))
    if paths["benchmark_gate_checks"] and os.path.exists(paths["benchmark_gate_checks"]):
        benchmark_gate_checks_rel = os.path.relpath(paths["benchmark_gate_checks"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_gate_checks_rel, benchmark_gate_checks_rel))
    if paths["benchmark_gate_result"] and os.path.exists(paths["benchmark_gate_result"]):
        benchmark_gate_result_rel = os.path.relpath(paths["benchmark_gate_result"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_gate_result_rel, benchmark_gate_result_rel))
    if paths["benchmark_gate_report"] and os.path.exists(paths["benchmark_gate_report"]):
        benchmark_gate_report_rel = os.path.relpath(paths["benchmark_gate_report"], os.path.join(ROOT, "docs")).replace("\\", "/")
        lines.append("- [docs/{}]({})".format(benchmark_gate_report_rel, benchmark_gate_report_rel))
    if paths["web_health"] and os.path.exists(paths["web_health"]):
        web_health_rel = os.path.relpath(paths["web_health"], ROOT).replace("\\", "/")
        lines.append("- [{}](../{})".format(web_health_rel, web_health_rel))
    if paths["web_index_head"] and os.path.exists(paths["web_index_head"]):
        web_index_head_rel = os.path.relpath(paths["web_index_head"], ROOT).replace("\\", "/")
        lines.append("- [{}](../{})".format(web_index_head_rel, web_index_head_rel))
    if paths["web_smoke_status"] and os.path.exists(paths["web_smoke_status"]):
        web_smoke_status_rel = os.path.relpath(paths["web_smoke_status"], ROOT).replace("\\", "/")
        lines.append("- [{}](../{})".format(web_smoke_status_rel, web_smoke_status_rel))
    if os.path.exists(paths["ctest_log"]):
        lines.append("- [{0}/ctest_full.log](../{0}/ctest_full.log)".format(run_dir_rel))
    rv32ui_p1_log = os.path.join(paths["run_dir"], "rv32ui_p1.log")
    rv32ui_p10_log = os.path.join(paths["run_dir"], "rv32ui_p10.log")
    rv32ui_nc_log = os.path.join(paths["run_dir"], "rv32ui_nocache.log")
    if os.path.exists(rv32ui_p1_log):
        lines.append("- [{0}/rv32ui_p1.log](../{0}/rv32ui_p1.log)".format(run_dir_rel))
    if os.path.exists(rv32ui_p10_log):
        lines.append("- [{0}/rv32ui_p10.log](../{0}/rv32ui_p10.log)".format(run_dir_rel))
    if os.path.exists(rv32ui_nc_log):
        lines.append("- [{0}/rv32ui_nocache.log](../{0}/rv32ui_nocache.log)".format(run_dir_rel))
    if os.path.exists(paths["bench_rc"]):
        lines.append("- [{0}/benchmark_rcs.csv](../{0}/benchmark_rcs.csv)".format(run_dir_rel))
    lines.append("")

    lines.append("## 9. 文件整理与清理建议")
    lines.append("")
    lines.append("### 9.1 建议归档（阶段性成果）")
    lines.append("")
    lines.append("- 建议目录：`archive/`（按 `stage-*` 分组，必要时在目录名加日期前缀）。")
    lines.append("- 建议归档：`tmp/*.log`、`tmp/fail_debug/*.out`、阶段性脚本与旧对比产物。")
    lines.append("- 建议归档说明文件包含：阶段目标、来源命令、时间、输入输出文件、复现方式。")
    lines.append("")
    lines.append("### 9.2 可立即删除（可重建）")
    lines.append("")
    lines.append("- `build/`（当前约 {}）：CMake 构建产物，可重新编译恢复。".format(build_size))
    lines.append("- `Testing/`（当前约 {}）：CTest 运行缓存。".format(testing_size))
    lines.append("- `{}`（属于 `tmp/` 的一部分，当前约 {}）：本轮原始日志目录。".format(run_dir_rel, tmp_size))
    lines.append("")
    lines.append("### 9.3 不建议删除")
    lines.append("")
    lines.append("- `src/`、`tests/`、`CMakeLists.txt`、`test_all.sh`：核心代码与测试入口。")
    lines.append("- `riscv-tests/`：官方 ISA 用例子模块。")
    lines.append("- `docs/FULL_TEST_PERFORMANCE_REPORT.md`、`docs/full_test_summary_*.csv`、`docs/figures/*.png`：最终可追溯结果。")
    lines.append("- `benchmarks/*.elf` 与 `benchmarks/quicksort_stress.c`：基准复现实验入口。")

    with open(paths["report_md"], "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")

    print("WROTE", paths["report_md"])
    print("WROTE", paths["summary_csv"])
    print("WROTE", paths["speedup_fig"])
    print("WROTE", paths["scatter_fig"])
    print("WROTE", paths["bench_fig"])


if __name__ == "__main__":
    main()
