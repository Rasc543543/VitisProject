#!/usr/bin/env python3
"""
check_hls_code.py - HLS C Code Static Analyzer
Usage: python scripts/check_hls_code.py
Output: terminal + review/check_report.txt
"""

import re, sys
from pathlib import Path
from datetime import datetime

# ============================================================
# 設定
# ============================================================
SCRIPT_DIR = Path(__file__).parent
ROOT_DIR   = SCRIPT_DIR.parent
REPORT_DIR = ROOT_DIR / "review"
REPORT_DIR.mkdir(exist_ok=True)
REPORT_OUT = REPORT_DIR / "check_report.txt"

# ============================================================
# 從 create_project.tcl 讀取 top function 與 source 檔案
# ============================================================
def parse_tcl_config():
    tcl = (SCRIPT_DIR / "create_project.tcl").read_text(errors="ignore")
    top   = re.search(r'set top_func\s+"([^"]+)"',   tcl)
    proj  = re.search(r'set project_name\s+"([^"]+)"', tcl)

    # resolve $root_dir / $script_dir as Python paths
    root_dir_str   = str(ROOT_DIR)
    script_dir_str = str(SCRIPT_DIR)

    def resolve(p: str) -> Path:
        p = p.replace("$root_dir",   root_dir_str)
        p = p.replace("$script_dir", script_dir_str)
        return Path(p)

    # 找所有 add_files（非 -tb）的 .cpp / .h
    src_files = re.findall(r'add_files\s+"([^"]+\.[ch](?:pp)?)"', tcl)
    tb_block  = re.findall(r'add_files\s+-tb\s+"([^"]+\.[ch](?:pp)?)"', tcl)
    return (
        top.group(1)  if top  else "unknown",
        proj.group(1) if proj else "unknown",
        [resolve(p) for p in src_files],
        [resolve(p) for p in tb_block],
    )

# ============================================================
# Finding 資料結構
# ============================================================
class Finding:
    def __init__(self, level, category, file, line, msg):
        self.level    = level     # ERROR / WARNING / INFO
        self.category = category  # PRAGMA / SYNTAX / RISK / TYPE / TB
        self.file     = file
        self.line     = line
        self.msg      = msg

    def __str__(self):
        loc = f"{Path(self.file).name}:{self.line}" if self.line else Path(self.file).name
        return f"[{self.level:<7}] [{self.category:<6}] {loc:30s}  {self.msg}"

findings = []

def add(level, category, file, line, msg):
    findings.append(Finding(level, category, file, line, msg))

# ============================================================
# Check 1: Pragma 完整性
# ============================================================
def check_pragmas(path: Path, top_func: str):
    text  = path.read_text(errors="ignore")
    lines = text.splitlines()

    # 找 top function 的宣告範圍
    in_top = False
    brace_depth = 0
    top_start = None

    for i, line in enumerate(lines, 1):
        stripped = line.strip()

        # 偵測 top function 開始
        if re.search(rf'\b{re.escape(top_func)}\s*\(', stripped) and not stripped.startswith("//"):
            in_top = True
            top_start = i

        if in_top:
            brace_depth += stripped.count('{') - stripped.count('}')
            if brace_depth < 0:
                in_top = False
                brace_depth = 0

    if top_start is None:
        add("INFO", "PRAGMA", path, None,
            f"Top function '{top_func}' not found in this file (may be in another file)")
        return

    # 取出 function body
    body = "\n".join(lines[top_start-1:])

    # --- 1a. Interface pragma ---
    has_interface = bool(re.search(r'#pragma\s+HLS\s+INTERFACE', body))
    if not has_interface:
        add("ERROR", "PRAGMA", path, top_start,
            f"Top function '{top_func}' has no #pragma HLS INTERFACE")

    # --- 1b. hls::stream ports 應該要有 axis interface ---
    stream_ports = re.findall(r'hls::stream\s*<[^>]+>\s*&?\s*(\w+)', body)
    for port in stream_ports:
        if not re.search(rf'INTERFACE\s+axis\s+port\s*=\s*{port}', body):
            add("WARNING", "PRAGMA", path, top_start,
                f"hls::stream port '{port}' may be missing 'axis' INTERFACE pragma")

    # --- 1c. return port 應該有 ap_ctrl ---
    if not re.search(r'INTERFACE\s+ap_ctrl', body):
        add("WARNING", "PRAGMA", path, top_start,
            f"No ap_ctrl_none/ap_ctrl_hs for 'return' port — default is ap_ctrl_hs")

    # --- 1d. Compute loop 應該有 PIPELINE ---
    loop_lines = [(i+top_start, l) for i, l in enumerate(lines[top_start-1:])
                  if re.search(r'\bfor\b|\bwhile\b|\bdo\b', l) and '//' not in l.split('for')[0]]
    pipeline_lines = set()
    for i, l in enumerate(lines[top_start-1:]):
        if re.search(r'#pragma\s+HLS\s+PIPELINE', l):
            pipeline_lines.add(i + top_start)

    if loop_lines and not pipeline_lines:
        add("WARNING", "PRAGMA", path, top_start,
            "Loops found but no #pragma HLS PIPELINE detected in top function")

    # --- 1e. BIND_STORAGE for ROM tables ---
    rom_arrays = re.findall(r'const\s+\w+\s+(\w+)\s*\[', body)
    for arr in rom_arrays:
        if not re.search(rf'BIND_STORAGE\s+variable\s*=\s*{arr}', body):
            add("INFO", "PRAGMA", path, top_start,
                f"const array '{arr}' has no BIND_STORAGE pragma (HLS will auto-choose)")

# ============================================================
# Check 2: HLS 禁用語法
# ============================================================
FORBIDDEN = [
    (r'\bnew\b\s+\w',         "ERROR",   "SYNTAX", "Dynamic allocation 'new' not supported in HLS synthesis"),
    (r'\bdelete\b\s+[\[\w]',  "ERROR",   "SYNTAX", "Dynamic allocation 'delete' not supported in HLS synthesis"),
    (r'\bmalloc\s*\(',        "ERROR",   "SYNTAX", "'malloc' not supported in HLS synthesis"),
    (r'\bfree\s*\(',          "ERROR",   "SYNTAX", "'free' not supported in HLS synthesis"),
    (r'\bvirtual\b',          "ERROR",   "SYNTAX", "'virtual' function not supported in HLS synthesis"),
    (r'\bdynamic_cast\b',     "ERROR",   "SYNTAX", "'dynamic_cast' not supported in HLS synthesis"),
    (r'\bstd::vector\b',      "ERROR",   "SYNTAX", "std::vector not synthesizable (dynamic size)"),
    (r'\bstd::map\b',         "ERROR",   "SYNTAX", "std::map not synthesizable"),
    (r'\bstd::string\b',      "WARNING", "SYNTAX", "std::string may cause synthesis issues"),
    (r'\brecursion\b|\brecursive\b', "WARNING", "SYNTAX", "Keyword 'recursive' detected — verify no recursive calls"),
]

def check_forbidden(path: Path):
    lines = path.read_text(errors="ignore").splitlines()
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        if stripped.startswith("//") or stripped.startswith("*"):
            continue
        # Skip inside #ifndef __SYNTHESIS__ blocks
        for pattern, level, cat, msg in FORBIDDEN:
            if re.search(pattern, line):
                add(level, cat, path, i, f"{msg}  →  '{line.strip()[:60]}'")

# ============================================================
# Check 3: 合成風險
# ============================================================
def check_synthesis_risks(path: Path):
    text  = path.read_text(errors="ignore")
    lines = text.splitlines()

    # --- 3a. printf / fprintf 沒有 #ifndef __SYNTHESIS__ 保護 ---
    in_synthesis_guard = False
    for i, line in enumerate(lines, 1):
        stripped = line.strip()
        if re.search(r'#ifndef\s+__SYNTHESIS__', stripped):
            in_synthesis_guard = True
        if re.search(r'#endif', stripped):
            in_synthesis_guard = False
        if not in_synthesis_guard:
            if re.search(r'\b(printf|fprintf|cout)\b', stripped) and not stripped.startswith("//"):
                add("WARNING", "RISK", path, i,
                    f"printf/cout outside #ifndef __SYNTHESIS__ guard — will be in RTL")

    # --- 3b. static 變數在 synthesis path ---
    for i, line in enumerate(lines, 1):
        if re.search(r'\bstatic\b', line) and not line.strip().startswith("//"):
            if not re.search(r'static\s+const', line):
                add("INFO", "RISK", path, i,
                    f"'static' variable → persistent register in RTL: '{line.strip()[:60]}'")

    # --- 3c. 不明確邊界的 while / do-while ---
    for i, line in enumerate(lines, 1):
        if re.search(r'\bwhile\s*\(', line) and not line.strip().startswith("//"):
            if not re.search(r'input_len\s*<|<\s*MAX_BUF|<\s*\d+|PIPELINE', line):
                add("WARNING", "RISK", path, i,
                    f"while loop — verify HLS can determine bound: '{line.strip()[:60]}'")

    # --- 3d. 自我遞迴呼叫 ---
    func_names = re.findall(r'\b(\w+)\s*\([^)]*\)\s*\{', text)
    for fname in set(func_names):
        # 找 function body 後是否呼叫自己
        pattern = rf'void\s+{re.escape(fname)}\s*\([^)]*\)\s*\{{(.*?)\}}'
        match = re.search(pattern, text, re.DOTALL)
        if match:
            body = match.group(1)
            calls = re.findall(rf'\b{re.escape(fname)}\s*\(', body)
            if calls:
                add("ERROR", "RISK", path, None,
                    f"Recursive call detected in function '{fname}'")

# ============================================================
# Check 4: 型別規範
# ============================================================
def check_types(path: Path):
    text  = path.read_text(errors="ignore")
    lines = text.splitlines()

    # --- 4a. 建議使用 ap_int / ap_uint 取代 int（top function 介面）---
    # 只在 header 或函式宣告行檢查
    for i, line in enumerate(lines, 1):
        if re.search(r'\bint\b\s+\w+\s*[,\)]', line) and not line.strip().startswith("//"):
            if re.search(r'(void|hls::stream)', line):
                add("INFO", "TYPE", path, i,
                    f"Interface uses plain 'int' — consider ap_int<N> for exact bit width")

    # --- 4b. ap_axiu 型別確認 ---
    if "hls::stream" in text and "ap_axiu" not in text and "ap_axis" not in text:
        add("WARNING", "TYPE", path, None,
            "hls::stream used but no ap_axiu/ap_axis type found — verify AXI-Stream data type")

# ============================================================
# Check 5: Testbench 覆蓋率
# ============================================================
def check_testbench(tb_paths: list, top_func: str):
    for path in tb_paths:
        if not path.exists():
            add("WARNING", "TB", path, None, f"Testbench file not found: {path}")
            continue

        text  = path.read_text(errors="ignore")
        lines = text.splitlines()

        # --- 5a. 有沒有呼叫 top function ---
        if top_func not in text:
            add("ERROR", "TB", path, None,
                f"Top function '{top_func}' not called in testbench")

        # --- 5b. 有沒有 golden 比對 ---
        has_compare = bool(re.search(r'!=|==|PASS|FAIL|error|mismatch', text, re.IGNORECASE))
        if not has_compare:
            add("WARNING", "TB", path, None,
                "No output comparison detected (PASS/FAIL or != check)")

        # --- 5c. 有沒有 edge case（最大錯誤數、空封包）---
        has_max_err = bool(re.search(r'max.*err|err.*max|9.*error|uncorrect', text, re.IGNORECASE))
        if not has_max_err:
            add("INFO", "TB", path, None,
                "No max-error / uncorrectable edge case detected")

        # --- 5d. 有沒有寫出 golden output 給 VCD 分析用 ---
        has_golden_write = bool(re.search(r'fopen.*["\'].*output|golden.*write|fprintf.*golden', text))
        if not has_golden_write:
            add("WARNING", "TB", path, None,
                "Testbench does not write golden output file — needed for analyze_vcd.py comparison")

# ============================================================
# 主程式
# ============================================================
def main():
    top_func, proj_name, src_files, tb_files = parse_tcl_config()

    print(f"\n{'='*60}")
    print(f"  HLS Code Check : {proj_name}  (top: {top_func})")
    print(f"{'='*60}")

    # 取得所有 src 檔案（若 TCL 裡沒有明確列出，掃描常見位置）
    if not src_files:
        candidates = list(ROOT_DIR.glob("src/*.cpp")) + \
                     list(ROOT_DIR.glob("src/*.h"))   + \
                     list(ROOT_DIR.glob("*.cpp"))     + \
                     list(ROOT_DIR.glob("*.h"))
        src_files = [f for f in candidates if "tb" not in f.name.lower()]

    if not tb_files:
        candidates = list(ROOT_DIR.glob("tb/*.cpp")) + \
                     list(ROOT_DIR.glob("tb_*.cpp"))  + \
                     list(ROOT_DIR.glob("**/tb_*.cpp"))
        tb_files = list(set(candidates))

    all_src = list({str(f): f for f in src_files}.values())

    print(f"\n  Source files : {[f.name for f in all_src]}")
    print(f"  TB files     : {[f.name for f in tb_files]}\n")

    for f in all_src:
        if not f.exists():
            add("WARNING", "RISK", f, None, f"Source file not found: {f}")
            continue
        check_pragmas(f, top_func)
        check_forbidden(f)
        check_synthesis_risks(f)
        check_types(f)

    check_testbench(tb_files, top_func)

    # ---- 輸出結果 ----
    errors   = [f for f in findings if f.level == "ERROR"]
    warnings = [f for f in findings if f.level == "WARNING"]
    infos    = [f for f in findings if f.level == "INFO"]

    lines_out = []
    lines_out.append(f"HLS Code Check Report")
    lines_out.append(f"Project : {proj_name}  (top: {top_func})")
    lines_out.append(f"Date    : {datetime.now().strftime('%Y-%m-%d %H:%M')}")
    lines_out.append("=" * 70)

    for group, label in [(errors, "ERRORS"), (warnings, "WARNINGS"), (infos, "INFO")]:
        if group:
            lines_out.append(f"\n--- {label} ({len(group)}) ---")
            for f in group:
                lines_out.append(str(f))

    summary = (f"\nSummary: {len(errors)} error(s), "
               f"{len(warnings)} warning(s), {len(infos)} info(s)")
    if errors:
        summary += "  →  [FAIL]"
    elif warnings:
        summary += "  →  [PASS with warnings]"
    else:
        summary += "  →  [PASS]"
    lines_out.append(summary)

    output = "\n".join(lines_out)
    print(output)

    REPORT_OUT.write_text(output, encoding="utf-8")
    print(f"\n[OK] Report saved: {REPORT_OUT}")

if __name__ == "__main__":
    main()
