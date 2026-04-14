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
    benchmark: str
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
    rows = []
    with open(path, newline="", encoding="utf-8") as f:
        rows.extend(csv.DictReader(f))
    return rows


def write_checks_csv(path, issues):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["severity", "benchmark", "metric", "value", "threshold", "message"])
        for it in issues:
            w.writerow([it.severity, it.benchmark, it.metric, f"{it.value:.6f}", f"{it.threshold:.6f}", it.message])


def write_report_md(path, status, issues, rows):
    lines = []
    lines.append("# Benchmark Gate Report")
    lines.append("")
    lines.append(f"- Overall status: **{status}**")
    lines.append("")
    lines.append("## Benchmark Summary")
    lines.append("")
    lines.append("| benchmark | rc_p1 | rc_p10 | rc_nocache | cycles_p1 | cycles_p10 | cycles_nocache | speedup_p10 | penalty_ratio_p10_over_p1 |")
    lines.append("|---|---:|---:|---:|---:|---:|---:|---:|---:|")
    for r in rows:
        lines.append(
            "| {} | {} | {} | {} | {} | {} | {} | {} | {} |".format(
                r.get("benchmark", "-"),
                r.get("rc_p1", "0"),
                r.get("rc_p10", "0"),
                r.get("rc_nocache", "0"),
                r.get("cycles_p1", "0"),
                r.get("cycles_p10", "0"),
                r.get("cycles_nocache", "0"),
                r.get("speedup_p10", "0"),
                r.get("penalty_ratio_p10_over_p1", "0"),
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
                    it.severity, it.benchmark, it.metric, it.value, it.threshold, it.message
                )
            )

    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")


def evaluate(
    rows,
    fail_min_speedup_p10,
    warn_min_speedup_p10,
    fail_max_penalty_ratio,
    warn_max_penalty_ratio,
):
    issues = []

    for row in rows:
        b = row.get("benchmark", "unknown")
        rc_p1 = to_int(row.get("rc_p1"), 127)
        rc_p10 = to_int(row.get("rc_p10"), 127)
        rc_nc = to_int(row.get("rc_nocache"), 127)

        if rc_p1 != 0:
            issues.append(Issue("FAIL", b, "rc_p1", rc_p1, 0, "benchmark p1 returned non-zero"))
        if rc_p10 != 0:
            issues.append(Issue("FAIL", b, "rc_p10", rc_p10, 0, "benchmark p10 returned non-zero"))
        if rc_nc != 0:
            issues.append(Issue("FAIL", b, "rc_nocache", rc_nc, 0, "benchmark nocache returned non-zero"))

        speedup_p10 = to_float(row.get("speedup_p10"), 0.0)
        if speedup_p10 < fail_min_speedup_p10:
            issues.append(Issue("FAIL", b, "speedup_p10", speedup_p10, fail_min_speedup_p10, "speedup below fail threshold"))
        elif speedup_p10 < warn_min_speedup_p10:
            issues.append(Issue("WARN", b, "speedup_p10", speedup_p10, warn_min_speedup_p10, "speedup below warning threshold"))

        penalty_ratio = to_float(row.get("penalty_ratio_p10_over_p1"), 0.0)
        if penalty_ratio > fail_max_penalty_ratio:
            issues.append(Issue("FAIL", b, "penalty_ratio_p10_over_p1", penalty_ratio, fail_max_penalty_ratio, "penalty ratio above fail threshold"))
        elif penalty_ratio > warn_max_penalty_ratio:
            issues.append(Issue("WARN", b, "penalty_ratio_p10_over_p1", penalty_ratio, warn_max_penalty_ratio, "penalty ratio above warning threshold"))

    has_fail = any(i.severity == "FAIL" for i in issues)
    has_warn = any(i.severity == "WARN" for i in issues)
    status = "FAIL" if has_fail else ("WARN" if has_warn else "PASS")
    code = 2 if has_fail else (1 if has_warn else 0)
    return status, code, issues


def main():
    ap = argparse.ArgumentParser(description="Benchmark regression gate checker")
    ap.add_argument("--summary", required=True, help="Path to benchmark_summary.csv")
    ap.add_argument("--fail-min-speedup-p10", type=float, default=1.20)
    ap.add_argument("--warn-min-speedup-p10", type=float, default=1.50)
    ap.add_argument("--fail-max-penalty-ratio", type=float, default=3.00)
    ap.add_argument("--warn-max-penalty-ratio", type=float, default=2.50)
    ap.add_argument("--output-prefix", default="")
    args = ap.parse_args()

    rows = read_summary(args.summary)
    status, code, issues = evaluate(
        rows,
        args.fail_min_speedup_p10,
        args.warn_min_speedup_p10,
        args.fail_max_penalty_ratio,
        args.warn_max_penalty_ratio,
    )

    if args.output_prefix:
        prefix = args.output_prefix
    else:
        base_dir = os.path.dirname(os.path.abspath(args.summary))
        prefix = os.path.join(base_dir, "benchmark_gate")

    checks_csv = prefix + "_checks.csv"
    result_json = prefix + "_result.json"
    report_md = prefix + "_report.md"

    write_checks_csv(checks_csv, issues)
    write_report_md(report_md, status, issues, rows)

    with open(result_json, "w", encoding="utf-8") as f:
        json.dump(
            {
                "status": status,
                "issues": [
                    {
                        "severity": i.severity,
                        "benchmark": i.benchmark,
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
    print("BENCHMARK_GATE_STATUS", status)
    sys.exit(code)


if __name__ == "__main__":
    main()
