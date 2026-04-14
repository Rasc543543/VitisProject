#!/usr/bin/env python3
"""
parse_report.py
Vitis HLS 2022.2 synthesis report 解析器
Usage: python scripts/parse_report.py [project_dir]
       python scripts/parse_report.py hls_project
"""

import os, sys, re, csv
from pathlib import Path


def parse_csynth_report(rpt_path: Path) -> dict:
    text = rpt_path.read_text(errors="ignore")
    m = {}
    m["file"] = rpt_path.name

    # Estimated clock: |ap_clk  |  10.00 ns|  8.750 ns|  1.25 ns|
    clk = re.search(r'\|ap_clk\s*\|[^|]+\|\s*([\d.]+)\s*ns', text)
    if clk:
        m["clock_est_ns"] = clk.group(1)

    # Latency summary row (after two header rows + separator)
    # |  Latency (cycles) | ...
    # |   min   |   max   | ...
    # +---------+...
    # |       34|       34|  0.340 us|  0.340 us|   35|   35|  no|
    lat = re.search(
        r'Latency \(cycles\).*?'           # 找到 Latency (cycles) 標題
        r'\+[-+]+\+\s*'                    # 分隔線
        r'\|\s*(\d+)\s*\|\s*(\d+)\s*\|'   # | min | max |
        r'[^|]*\|[^|]*\|'                 # 跳過 absolute latency
        r'\s*(\d+)\s*\|\s*(\d+)\s*\|',    # | interval_min | interval_max |
        text, re.DOTALL
    )
    if lat:
        m["lat_min"]      = lat.group(1)
        m["lat_max"]      = lat.group(2)
        m["interval_min"] = lat.group(3)
        m["interval_max"] = lat.group(4)

    # Resources Total row: |Total  | BRAM_18K | DSP | FF | LUT | URAM |
    res = re.search(
        r'\|Total\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|',
        text
    )
    if res:
        m["BRAM_18K"] = res.group(1)
        m["DSP"]      = res.group(2)
        m["FF"]       = res.group(3)
        m["LUT"]      = res.group(4)
        m["URAM"]     = res.group(5)

    return m


def print_table(metrics_list: list):
    if not metrics_list:
        return
    keys = list(metrics_list[0].keys())
    col_w = {k: max(len(k), max(len(str(m.get(k, "-"))) for m in metrics_list)) for k in keys}
    header = "  ".join(k.ljust(col_w[k]) for k in keys)
    sep    = "  ".join("-" * col_w[k] for k in keys)
    print("\n" + sep)
    print(header)
    print(sep)
    for m in metrics_list:
        print("  ".join(str(m.get(k, "-")).ljust(col_w[k]) for k in keys))
    print(sep)


def main():
    project_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")

    rpt_files = list(project_dir.rglob("*_csynth.rpt"))

    if not rpt_files:
        print(f"[WARN] No synthesis reports found under: {project_dir.resolve()}")
        print("       Run synthesis first (RUN_SYN=1 in run_all.tcl)")
        return

    all_metrics = [parse_csynth_report(f) for f in rpt_files]
    print_table(all_metrics)

    # Save CSV
    csv_out = project_dir / "output" / "report_summary.csv"
    csv_out.parent.mkdir(exist_ok=True)
    keys = list(all_metrics[0].keys())
    with open(csv_out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=keys, extrasaction="ignore")
        w.writeheader()
        w.writerows(all_metrics)

    print(f"\n[OK] CSV saved: {csv_out.resolve()}")


if __name__ == "__main__":
    main()
