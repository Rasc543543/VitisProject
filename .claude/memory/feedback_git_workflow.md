---
name: Git 版本控制工作流程
description: VitisProject 已初始化 git，合成產物排除在外，改完測試後 commit
type: feedback
originSessionId: 47690edc-2cde-4b74-a4cc-d5b12fea4ac5
---
Git repo 已初始化在 `C:/VitisProject`（2026-04-14），GitHub remote 設定完成（2026-05-13）。

**GitHub remote：** `https://github.com/Rasc543543/VitisProject.git`（private repo）

**Why:** 需要跨機器同步開發環境與 Claude 記憶檔。

**How to apply:**
- 改完 IP 且測試通過後，才 commit（`git add <IP路徑>/ && git commit -m "fix(...): ..."`)
- `.gitignore` 已排除：`solution*/impl/`、`solution*/sim/`、`output/`、`.jou`、`.str`、`vivado_sim/`、`*.backup.log`、`IPbackup/`、`xsa_bitstream_*/`、`*.bit`、`*.xsa`
- 合成產物（RTL netlist）不 commit，可由 `run_all.tcl` 重新產生
- commit message 格式：`fix(ref_rx): 說明` 或 `feat(rx_rs): 說明`

**跨機器同步流程：**
- 記憶檔備份在 repo 的 `.claude/memory/`，push/pull 隨程式碼一起同步
- 新機器 clone 後執行 `.\setup_claude_memory.ps1` 一次，複製記憶檔到本機 Claude 路徑
- 記憶檔有更新時：`git add .claude\memory\ && git commit -m "sync: update memory" && git push`
- CLAUDE.md 在專案根目錄，Claude Code 每次開啟自動載入靜態背景知識
