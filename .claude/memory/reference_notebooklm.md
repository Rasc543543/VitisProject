---
name: NotebookLM 安裝與 CFO 論文 Notebook
description: notebooklm-py 已安裝完成，CFO 論文 notebook 與五篇精選分析文件位置
type: reference
originSessionId: 8e3df132-610e-4912-b661-d22cbb99f6fb
---
## 安裝狀態（2026-05-10 完成）

- `notebooklm-py` 0.4.0 已安裝（pip）
- playwright chromium 已安裝
- Google 登入完成，session 存於 `C:\Users\ASUS\.notebooklm\profiles\default\storage_state.json`
- Skill 已部署：`~/.claude/skills/notebooklm/SKILL.md`
- CLI 可直接使用：`notebooklm list`、`notebooklm use <id>`、`notebooklm ask "..."`

## CFO 論文 Notebook

- **名稱：** Zadoff-Chu Synchronization for Short-Burst OFDM Signals
- **Notebook ID：** `4c3a7ef2-12f9-44f7-8238-52b64a346b18`（縮寫：`4c3a7ef2`）
- **論文數量：** 21 篇 PDF，全部 ready
- **已探查日期：** 2026-05-10

## 五篇精選論文（與 ref_rx_packed CFO 優化直接相關）

| 優先度 | 論文 | 可用技術 |
|--------|------|---------|
| 立即 | Classen CFO Synchronization | CFO cache 學術名稱（capture+tracking two-mode）|
| 立即 | SAL Algorithm（鄰近 subcarrier） | energy 計算改 abs，LUT -90%, FF -94% |
| 中期 | FFT-Based ICFO（802.15.4g） | FFT cross-correlator 28.4× 速度，需 training symbol |
| RF 後 | Decision Feedback Estimator | 消除 ICI error floor（SNR>14dB）|
| RF 後 | Easy Hardware LS（802.11a） | Fractional CFO shift-add，DSP -75% |

## 分析文件位置

- HTML（瀏覽器閱讀）：`C:\VitisProject\design_notes\cfo_papers_deep_dive.html`
- Markdown（VS Code / git）：`C:\VitisProject\design_notes\cfo_papers_deep_dive.md`

## 使用方式

```powershell
# 切換到 CFO notebook
notebooklm use 4c3a7ef2

# 查詢
notebooklm ask "你的問題"
```

## Session 失效時的處理

```powershell
notebooklm login   # 重新登入，重新儲存 session
```
