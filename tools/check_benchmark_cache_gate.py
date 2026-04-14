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
    lines.append("# Benchmark Cache Matrix Gate Report")
    lines.append("")
    lines.append(f"- Overall status: **{status}**")
    lines.append(f"- Baseline policy: `{baseline}`")
    lines.append("")
    lines.append("## Policy Summary")
    lines.append("")
    lines.append("| policy | pass/benchmarks | avg_cycles | avg_i_hit_pct | avg_d_hit_pct | avg_speedup_vs_nocache |")
    lines.append("|---|---:|---:|---:|---:|---:|")
    for policy in sorted(rows.keys()):
        row = rows[policy]
        bench_count = to_int(row.get("benchmarks", row.get("tests", 0)))
        lines.append(
            "| {} | {}/{} | {} | {} | {} | {} |".format(
                policy,
                to_int(row.get("pass")),
                bench_count,
                row.get("avg_cycles", "0"),
                row.get("avg_i_hit_pct", "0"),
                row.get("avg_d_hit_pct", "0"),
                row.get("avg_speedup_vs_nocache", "0"),
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


def evaluate(
    rows,
    baseline,
    fail_cycle_regress_pct,
    warn_cycle_regress_pct,
    fail_speedup_drop_pct,
    warn_speedup_drop_pct,
    fail_dhit_drop_pct,
    warn_dhit_drop_pct,
):
    issues = []

    base = rows.get(baseline)
    if not base:
        raise RuntimeError(f"baseline policy not found: {baseline}")

    base_cycles = to_float(base.get("avg_cycles"))
    base_speedup = to_float(base.get("avg_speedup_vs_nocache"))
    base_dhit = to_float(base.get("avg_d_hit_pct"))

    for policy, row in rows.items():
        bench_count = to_int(row.get("benchmarks", row.get("tests", 0)))
        passed = to_int(row.get("pass"))
        failed = to_int(row.get("fail"))
        is_nocache = policy.lower() == "nocache"

        if bench_count <= 0:
            issues.append(Issue("FAIL", policy, "benchmarks", bench_count, 1.0, "no benchmark rows recorded"))
            continue
        if failed > 0 or passed < bench_count:
            issues.append(Issue("FAIL", policy, "pass_rate", passed / bench_count, 1.0, "benchmark case returned non-zero rc"))

        cycles = to_float(row.get("avg_cycles"))
        speedup = to_float(row.get("avg_speedup_vs_nocache"))
        d_hit = to_float(row.get("avg_d_hit_pct"))

        if policy != baseline and (not is_nocache):
            if base_cycles > 0:
                regress = ((cycles - base_cycles) / base_cycles) * 100.0
                if regress > fail_cycle_regress_pct:
                    issues.append(Issue("FAIL", policy, "cycles_regress_pct", regress, fail_cycle_regress_pct, "average cycles regression beyond fail threshold"))
                elif regress > warn_cycle_regress_pct:
                    issues.append(Issue("WARN", policy, "cycles_regress_pct", regress, warn_cycle_regress_pct, "average cycles regression warning"))

            if base_speedup > 0:
                speedup_drop = ((base_speedup - speedup) / base_speedup) * 100.0
                if speedup_drop > fail_speedup_drop_pct:
                    issues.append(Issue("FAIL", policy, "speedup_drop_pct", speedup_drop, fail_speedup_drop_pct, "speedup drop beyond fail threshold"))
                elif speedup_drop > warn_speedup_drop_pct:
                    issues.append(Issue("WARN", policy, "speedup_drop_pct", speedup_drop, warn_speedup_drop_pct, "speedup drop warning"))

            d_hit_drop = base_dhit - d_hit
            if d_hit_drop > fail_dhit_drop_pct:
                issues.append(Issue("FAIL", policy, "d_hit_drop_pct", d_hit_drop, fail_dhit_drop_pct, "D-cache hit-rate drop beyond fail threshold"))
            elif d_hit_drop > warn_dhit_drop_pct:
                issues.append(Issue("WARN", policy, "d_hit_drop_pct", d_hit_drop, warn_dhit_drop_pct, "D-cache hit-rate drop warning"))

    has_fail = any(i.severity == "FAIL" for i in issues)
    has_warn = any(i.severity == "WARN" for i in issues)
    status = "FAIL" if has_fail else ("WARN" if has_warn else "PASS")
    code = 2 if has_fail else (1 if has_warn else 0)
    return status, code, issues


def main():
    ap = argparse.ArgumentParser(description="Benchmark cache matrix regression gate checker")
    ap.add_argument("--summary", required=True, help="Path to benchmark_policy_summary.csv")
    ap.add_argument("--baseline", default="wb_wa", help="Baseline policy id")
    ap.add_argument("--fail-cycle-regress-pct", type=float, default=10.0)
    ap.add_argument("--warn-cycle-regress-pct", type=float, default=5.0)
    ap.add_argument("--fail-speedup-drop-pct", type=float, default=10.0)
    ap.add_argument("--warn-speedup-drop-pct", type=float, default=5.0)
    ap.add_argument("--fail-dhit-drop-pct", type=float, default=10.0)
    ap.add_argument("--warn-dhit-drop-pct", type=float, default=6.0)
    ap.add_argument("--output-prefix", default="")
    args = ap.parse_args()

    rows = read_summary(args.summary)
    status, code, issues = evaluate(
        rows,
        args.baseline,
        args.fail_cycle_regress_pct,
        args.warn_cycle_regress_pct,
        args.fail_speedup_drop_pct,
        args.warn_speedup_drop_pct,
        args.fail_dhit_drop_pct,
        args.warn_dhit_drop_pct,
    )

    if args.output_prefix:
        prefix = args.output_prefix
    else:
        base_dir = os.path.dirname(os.path.abspath(args.summary))
        prefix = os.path.join(base_dir, "benchmark_cache_gate")

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
    print("BENCHMARK_CACHE_GATE_STATUS", status)
    sys.exit(code)


if __name__ == "__main__":
    main()
