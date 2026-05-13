# NotebookLM 概覽

## 是什麼

Google 提供的 AI 研究助理，核心功能是把你上傳的文件（PDF、URL、影片字幕等）建成向量索引，之後問問題只在文件內搜尋並回答，每個答案帶 [引用] 指向原文段落。

**跟 ChatGPT/Claude 的根本差異：**
- 只從你自己的文件回答，不會外推或幻覺
- 文件原文不進 LLM context，由 Google 後端做向量檢索
- 算力全部由 Google 吸收，對用戶免費

---

## 方案限制（2026 年）

| 項目 | 免費版 | Plus | Pro | Ultra |
|------|--------|------|-----|-------|
| 每個 notebook 最多 sources | 50 | 300 | 300 | 600 |
| 每個 source 最大字數 | 500,000 words | 同左 | 同左 | 同左 |
| 每個 source 最大檔案 | 200 MB | 同左 | 同左 | 同左 |
| 每日聊天查詢次數 | 50 次 | 更多 | 更多 | 更多 |
| 每日音頻生成次數 | 3 次 | 更多 | 更多 | 更多 |
| 最多 notebook 數量 | 100 | 更多 | 更多 | 更多 |

**對 OFDM 研究的意義：**
- 免費版 50 個 sources 已夠用（一個研究主題通常 10~30 篇論文）
- 500,000 words/source 幾乎不可能觸及（一篇 IEEE paper 約 5,000~15,000 words）

---

## 支援的 Source 格式

| 類型 | 格式 |
|------|------|
| 本地檔案 | PDF、DOCX、TXT、MP3、影片檔 |
| 網路來源 | URL（任意網頁）、YouTube 字幕 |
| Google 服務 | Google Docs、Google Slides |
| 直接貼上 | 純文字（複製貼上）|
| 圖片 | 圖片 OCR |

**不支援：**
- 受版權保護的 PDF（加密/DRM）
- 需要登入才能存取的頁面

---

## 核心機制：為什麼能省 Claude Token

```
傳統做法（塞 context）：
  Claude input = 系統提示 + 對話歷史 + 50,000 字論文 × N 篇
  每輪問答都要計算整個 context

NotebookLM + Claude：
  NotebookLM 做向量搜尋 → 返回 300~1,000 字相關段落
  Claude input = 系統提示 + 對話歷史 + 300~1,000 字答案
  論文原文「從未進入」Claude 的 input token
```

**實測數字（47 篇論文，5 輪問答，Opus 4.7）：**
- 傳統做法（開 cache，最優情境）：~$9.59
- NotebookLM + Claude：~$0.55
- **差距：17 倍**

---

## 重要注意事項

1. **非官方 client 風險：** 所有第三方 client 都是逆向工程，Google 改後端可能失效
2. **Session 安全：** `storage_state.json` 或登入 cookie 是你的 Google 活 session，不要上傳到 git 或分享給他人
3. **速度代價：** NotebookLM 查詢約 16~48 秒（中位 45 秒），直接問 Claude 約 20~35 秒，慢約 3 倍

---

## 參考來源

- [NotebookLM 官方方案說明](https://notebooklm.google/plans)
- [NotebookLM 常見問題](https://support.google.com/notebooklm/answer/16269187)
- [方案限制詳細比較](https://www.abisheklakandri.com/blog/notebooklm-tiers-pricing-guide-free-plus-pro-ultra-2026)
