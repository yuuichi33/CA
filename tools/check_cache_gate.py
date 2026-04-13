#!/usr/bin/env python3
import argparse
import csv
import json
import os
import sys
from dataclasses import dataclass


@dataclass
class Issue:
    severity: str
    policy: str
    metric: str
    value: float
    threshold: float
    message: str


def to_float(v, d=0.0):
    try:
        return float(v)
    except Exception:
        return d


def to_int(v, d=0):
    try:
        return int(float(v))
    except Exception:
        return d


def read_summary(path):
    rows = {}
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rows[row["policy"]] = row
    return rows


def write_checks_csv(path, issues):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["severity", "policy", "metric", "value", "threshold", "message"])
        for it in issues:
            w.writerow([it.severity, it.policy, it.metric, f"{it.value:.6f}", f"{it.threshold:.6f}", it.message])


def write_report_md(path, status, baseline, issues, rows):
    lines = []
    lines.append("# Cache Gate Report")
    lines.append("")
    lines.append(f"- Overall status: **{status}**")
    lines.append(f"- Baseline policy: `{baseline}`")
    lines.append("")
    lines.append("## Policy Summary")
    lines.append("")
    lines.append("| policy | pass/tests | avg_cycles | avg_i_hit_pct | avg_d_hit_pct | avg_speedup_vs_nocache |")
    lines.append("|---|---:|---:|---:|---:|---:|")
    for p in sorted(rows.keys()):
        r = rows[p]
        lines.append(
            "| {} | {}/{} | {} | {} | {} | {} |".format(
                p,
                to_int(r.get("pass")),
                to_int(r.get("tests")),
                r.get("avg_cycles", "0"),
                r.get("avg_i_hit_pct", "0"),
                r.get("avg_d_hit_pct", "0"),
                r.get("avg_speedup_vs_nocache", "0"),
            )
        )
    lines.append("")
    lines.append("## Issues")
    lines.append("")
    if not issues:
        lines.append("- None")
    else:
        for it in issues:
            lines.append(
                "- [{}] {} {} value={:.4f} threshold={:.4f} ({})".format(
                    it.severity, it.policy, it.metric, it.value, it.threshold, it.message
                )
            )

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def evaluate(rows, baseline, max_cycle_regress_pct, max_ihit_drop_pct, max_dhit_drop_pct, min_cache_stall_ratio):
    issues = []
    base = rows.get(baseline)
    if not base:
        raise RuntimeError(f"baseline policy not found: {baseline}")

    b_cycles = to_float(base.get("avg_cycles"))
    b_i = to_float(base.get("avg_i_hit_pct"))
    b_d = to_float(base.get("avg_d_hit_pct"))

    for p, row in rows.items():
        tests = to_int(row.get("tests"))
        passed = to_int(row.get("pass"))
        failed = to_int(row.get("fail"))
        is_nocache = p.lower() == "nocache"

        if tests <= 0:
            issues.append(Issue("FAIL", p, "tests", tests, 1, "no tests recorded"))
            continue
        if failed > 0 or passed < tests:
            issues.append(Issue("FAIL", p, "pass_rate", passed / tests, 1.0, "functional regression (rc!=0 exists)"))

        cycles = to_float(row.get("avg_cycles"))
        i_hit = to_float(row.get("avg_i_hit_pct"))
        d_hit = to_float(row.get("avg_d_hit_pct"))
        avg_stall = to_float(row.get("avg_stall"))
        avg_cache_stall = to_float(row.get("avg_cache_stall"))

        if (p != baseline) and (not is_nocache) and b_cycles > 0:
            regress = (cycles - b_cycles) / b_cycles * 100.0
            if regress > max_cycle_regress_pct:
                issues.append(Issue("FAIL", p, "cycles_regress_pct", regress, max_cycle_regress_pct, "cycles regression beyond threshold"))
            elif regress > (max_cycle_regress_pct / 2.0):
                issues.append(Issue("WARN", p, "cycles_regress_pct", regress, max_cycle_regress_pct / 2.0, "cycles regression warning"))

            i_drop = b_i - i_hit
            if i_drop > max_ihit_drop_pct:
                issues.append(Issue("FAIL", p, "i_hit_drop_pct", i_drop, max_ihit_drop_pct, "I-cache hit rate drop beyond threshold"))
            elif i_drop > (max_ihit_drop_pct / 2.0):
                issues.append(Issue("WARN", p, "i_hit_drop_pct", i_drop, max_ihit_drop_pct / 2.0, "I-cache hit rate drop warning"))

            d_drop = b_d - d_hit
            if d_drop > max_dhit_drop_pct:
                issues.append(Issue("FAIL", p, "d_hit_drop_pct", d_drop, max_dhit_drop_pct, "D-cache hit rate drop beyond threshold"))
            elif d_drop > (max_dhit_drop_pct / 2.0):
                issues.append(Issue("WARN", p, "d_hit_drop_pct", d_drop, max_dhit_drop_pct / 2.0, "D-cache hit rate drop warning"))

        if (not is_nocache) and avg_stall > 0:
            ratio = avg_cache_stall / avg_stall
            if ratio < min_cache_stall_ratio:
                issues.append(Issue("WARN", p, "cache_stall_ratio", ratio, min_cache_stall_ratio, "cache stall ratio unexpectedly low"))

    has_fail = any(i.severity == "FAIL" for i in issues)
    has_warn = any(i.severity == "WARN" for i in issues)
    status = "FAIL" if has_fail else ("WARN" if has_warn else "PASS")
    code = 2 if has_fail else (1 if has_warn else 0)
    return status, code, issues


def main():
    ap = argparse.ArgumentParser(description="Cache regression gate checker")
    ap.add_argument("--summary", required=True, help="Path to policy_summary.csv")
    ap.add_argument("--baseline", default="wb_wa", help="Baseline policy id")
    ap.add_argument("--max-cycle-regress-pct", type=float, default=15.0)
    ap.add_argument("--max-ihit-drop-pct", type=float, default=5.0)
    ap.add_argument("--max-dhit-drop-pct", type=float, default=8.0)
    ap.add_argument("--min-cache-stall-ratio", type=float, default=0.20)
    ap.add_argument("--output-prefix", default="")
    args = ap.parse_args()

    rows = read_summary(args.summary)
    status, code, issues = evaluate(
        rows,
        args.baseline,
        args.max_cycle_regress_pct,
        args.max_ihit_drop_pct,
        args.max_dhit_drop_pct,
        args.min_cache_stall_ratio,
    )

    if args.output_prefix:
        prefix = args.output_prefix
    else:
        base_dir = os.path.dirname(os.path.abspath(args.summary))
        prefix = os.path.join(base_dir, "gate")

    checks_csv = prefix + "_checks.csv"
    result_json = prefix + "_result.json"
    report_md = prefix + "_report.md"

    write_checks_csv(checks_csv, issues)
    write_report_md(report_md, status, args.baseline, issues, rows)

    with open(result_json, "w", encoding="utf-8") as f:
        json.dump(
            {
                "status": status,
                "baseline": args.baseline,
                "issues": [
                    {
                        "severity": i.severity,
                        "policy": i.policy,
                        "metric": i.metric,
                        "value": i.value,
                        "threshold": i.threshold,
                        "message": i.message,
                    }
                    for i in issues
                ],
            },
            f,
            ensure_ascii=False,
            indent=2,
        )

    print("WROTE", checks_csv)
    print("WROTE", report_md)
    print("WROTE", result_json)
    print("GATE_STATUS", status)
    sys.exit(code)


if __name__ == "__main__":
    main()
