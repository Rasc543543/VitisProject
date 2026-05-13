# NotebookLM Client 方案比較

目前有三個主要的第三方 NotebookLM client，可以讓 Claude Code 存取 NotebookLM。

---

## 方案一：notebooklm-client（icebear0828）

**GitHub：** https://github.com/icebear0828/notebooklm-client  
**語言：** TypeScript / Node.js  
**安裝方式：** npm

```bash
npm i notebooklm-client
npx notebooklm export-session   # 開瀏覽器登 Google
npx notebooklm skill install    # 裝進 Claude Code（/notecraft 命令）
```

**版本：** 0.2.0  
**授權：** MIT  
**需求：** Node.js >= 20、Google Chrome

**可用 CLI 命令：**

| 命令 | 功能 |
|------|------|
| `export-session` | 瀏覽器登入並儲存 session |
| `list` | 列出所有 notebooks |
| `chat` | 與 notebook sources 對話 |
| `audio` | 生成 podcast（deep_dive / brief / critique / debate）|
| `report` | 生成文件（briefing / study guide / blog）|
| `video` | 生成影片概覽 |
| `quiz` / `flashcards` | 生成學習材料 |
| `slides` | 生成簡報 |
| `infographic` | 生成資訊圖表 |
| `data-table` | 生成結構化資料 |
| `analyze` | 分析問答 |

**Claude Code 整合：** 裝完後用 `/notecraft chat` 命令  
**Windows 支援：** 文件無特別說明，理論上支援（Node.js 跨平台）

**已知問題：**

| 問題 | 解決方式 |
|------|---------|
| "No session available" | 重跑 `npx notebooklm export-session` |
| Session 過期 | 重跑 export-session |
| 連線逾時 | 加 `--proxy socks5://127.0.0.1:7890` 或設 `HTTPS_PROXY` |

---

## 方案二：notebooklm-skill（PleasePrompto）

**GitHub：** https://github.com/PleasePrompto/notebooklm-skill  
**語言：** Python  
**安裝方式：** git clone

```bash
mkdir -p ~/.claude/skills
cd ~/.claude/skills
git clone https://github.com/PleasePrompto/notebooklm-skill notebooklm
```

**特色：**
- 裝完後第一次使用時自動建立 Python venv 並安裝依賴
- 透過自然語言操作（說「查我的 React docs 裡關於 X 的部分」）
- Claude Code 自動讀取 `SKILL.md` 載入技能說明

**使用方式（自然語言）：**
- "Set up NotebookLM authentication" → 開 Chrome 登入
- "Add [notebook link] to my library" → 新增 notebook
- "Show my NotebookLM notebooks" → 列出已存
- "What does my notebook say about [topic]?" → 查詢

**Windows 支援：** ✅ 明確支援，venv 啟動用 `.venv\Scripts\activate`

**限制：**
- 只支援本地 Claude Code CLI，不支援 Web UI（沙盒限制）

---

## 方案三：notebooklm-py（teng-lin）★ 推薦

**GitHub：** https://github.com/teng-lin/notebooklm-py  
**語言：** Python  
**安裝方式：** pip

```bash
pip install "notebooklm-py[browser]"
playwright install chromium
notebooklm login                 # 登入 Google
notebooklm skill install         # 一鍵裝進 Claude Code
```

**版本：** CI/CD 持續測試  
**Windows 支援：** ✅ 明確測試通過

**可用 CLI 命令：**

| 命令 | 功能 |
|------|------|
| `notebooklm login` | 登入 Google 帳號 |
| `notebooklm create "name"` | 建立新 notebook |
| `notebooklm source add <url>` | 加入 source（URL/PDF/影片）|
| `notebooklm ask "question"` | 查詢 sources |
| `notebooklm generate audio` | 生成 podcast |
| `notebooklm generate video` | 生成影片 |
| `notebooklm generate quiz` | 生成測驗 |
| `notebooklm generate flashcards` | 生成學習卡 |
| `notebooklm download <type>` | 下載輸出（JSON/CSV/PPTX）|
| `notebooklm skill install` | 裝進 Claude Code |

**另一種安裝方式（npm）：**
```bash
npx skills add teng-lin/notebooklm-py
```

---

## 三方案比較表

| 項目 | notebooklm-client | notebooklm-skill | notebooklm-py ★ |
|------|:-----------------:|:----------------:|:---------------:|
| 語言 | Node.js | Python | Python |
| Windows 測試 | 理論支援 | ✅ 明確 | ✅ CI 測試 |
| 一鍵安裝 | npm install | git clone | pip install |
| Skill 安裝 | `skill install` | 手動 clone | `skill install` |
| 功能完整度 | 完整 | 基本查詢 | 完整 + 下載 |
| 活躍維護 | 有 | 有 | 有 |
| 文件品質 | 完整 | 完整 | 完整 |

**推薦選 notebooklm-py** 原因：
1. Python 生態，跟 Windows 相容性問題較少
2. CI/CD 明確測試 Windows
3. `notebooklm skill install` 最簡潔
4. 支援 batch 下載，方便本地存檔
