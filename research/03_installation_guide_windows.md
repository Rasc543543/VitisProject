# Windows 安裝教學（推薦方案：notebooklm-py）

## 前置需求

- Python 3.9+（`python --version` 確認）
- pip 可用
- Google 帳號（有 NotebookLM 存取權限）

---

## Step 1：安裝 notebooklm-py

```powershell
pip install "notebooklm-py[browser]"
playwright install chromium
```

`[browser]` 選項會安裝 Playwright，用於瀏覽器自動化登入。

---

## Step 2：登入 Google

```powershell
notebooklm login
```

這會開啟 Chrome，讓你登入 Google 帳號。登入後 session 會儲存在本地設定檔。

**⚠️ 安全注意：** session 檔案等同於你的 Google 登入狀態，不要：
- 上傳到 GitHub
- 分享給他人
- 放在雲端同步的資料夾

---

## Step 3：裝進 Claude Code

```powershell
notebooklm skill install
```

這會把 skill 部署到 `~/.claude/skills/notebooklm`，之後在 Claude Code 對話裡就能直接使用。

---

## Step 4：在 NotebookLM 網站建立 Notebook

1. 前往 https://notebooklm.google.com/
2. 建立新 Notebook（例如「CFO Estimation Papers」）
3. 上傳 PDF 論文或貼上 IEEE 論文 URL
4. 記下 Notebook 的 ID（網址列的那串）

---

## Step 5：測試

在 Claude Code 對話裡說：

```
Show my NotebookLM notebooks
```

或直接問：
```
What does my CFO estimation notebook say about integer frequency offset estimation?
```

Claude 會自動呼叫 skill 去 NotebookLM 查，然後把帶引用的答案回傳到你的對話。

---

## 可能遇到的問題

| 問題 | 解決方式 |
|------|---------|
| `playwright install` 失敗 | 確認 pip 版本，或改用 `pip install playwright` 再 `python -m playwright install chromium` |
| 登入後 session 失效 | 重跑 `notebooklm login` |
| Skill 裝完 Claude 看不到 | 重啟 Claude Code，或確認 `~/.claude/skills/` 目錄存在 |
| 查詢逾時 | 網路問題，NotebookLM 回應約 16~48 秒，屬正常範圍 |

---

## 備選方案：notebooklm-client（Node.js 版）

如果 Python 環境有問題，改用 Node.js 版：

```powershell
npm i notebooklm-client
npx notebooklm export-session
npx notebooklm skill install
```

裝完後 Claude Code 裡用 `/notecraft chat <notebook-id> --question "你的問題"` 查詢。

---

## 備選方案二：notebooklm-skill（git clone 版）

```powershell
# 確認 ~/.claude/skills 目錄存在
mkdir $HOME\.claude\skills -ErrorAction SilentlyContinue

cd $HOME\.claude\skills
git clone https://github.com/PleasePrompto/notebooklm-skill notebooklm
```

裝完後在 Claude Code 說 "Set up NotebookLM authentication" 開始設定。
