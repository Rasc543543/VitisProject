# NotebookLM × Claude Code 研究資料夾

**目的：** 研究如何用 NotebookLM 做 RAG，讓 IEEE 論文原文不進 Claude context，大幅省 token。

## 文件清單

| 檔案 | 內容 |
|------|------|
| `01_notebooklm_overview.md` | NotebookLM 是什麼、各方案限制、支援格式 |
| `02_clients_comparison.md` | 三個可用 client 的比較（npm / Python pip / git clone）|
| `03_installation_guide_windows.md` | Windows 逐步安裝教學（推薦方案）|
| `04_ofdm_research_workflow.md` | 針對 OFDM / IEEE 論文的具體工作流程 |

## 快速結論

- **推薦 client：** `notebooklm-py`（teng-lin），Python 版，Windows ✅ 測試通過，有 `notebooklm skill install` 一鍵裝進 Claude Code
- **節省幅度：** 同樣 5 輪研究會話，費用約為傳統塞 context 方式的 1/17
- **代價：** 每次查詢比直接問 Claude 慢約 45 秒

建立日期：2026-05-10
