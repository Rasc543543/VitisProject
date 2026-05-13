---
name: Git 版本控制工作流程
description: VitisProject 已初始化 git，合成產物排除在外，改完測試後 commit
type: feedback
originSessionId: 47690edc-2cde-4b74-a4cc-d5b12fea4ac5
---
Git repo 已初始化在 `C:/VitisProject`（2026-04-14，commit `a33a857`）。

**Why:** 沒有 git history 導致無法追蹤每個 IP 的修改歷程，debug 困難。

**How to apply:**
- 改完 IP 且測試通過後，才 commit（`git add <IP路徑>/ && git commit -m "fix(...): ..."`)
- `.gitignore` 已排除：`solution*/impl/`、`solution*/sim/`、`output/`、`.jou`、`.str`、`vivado_sim/`、`*.backup.log`
- 合成產物（RTL netlist）不 commit，可由 `run_all.tcl` 重新產生
- commit message 格式：`fix(ref_rx): 說明` 或 `feat(rx_rs): 說明`
