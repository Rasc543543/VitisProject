# OFDM 研究工作流程：NotebookLM × Claude Code

## 使用情境

你在研究 CFO estimation / OFDM 同步相關的 IEEE 論文，需要：
- 查哪篇論文用什麼方法
- 比較不同方法的優劣
- 找到跟你目前 HLS 實作對應的學術依據

這些操作如果讓 Claude 直接看論文原文，每篇 ~10,000 字，10 篇就是 100,000 token，一輪問答就吃掉大量額度。

---

## 建議的 Notebook 結構

建議按主題建 Notebook，不要把所有論文放同一個：

| Notebook 名稱 | 放入的論文 |
|--------------|-----------|
| CFO Synchronization | Schmidl & Cox (1997)、Moose (1994)、van de Beek (1997)、Morelli & Mengali (1999) |
| OFDM Channel Estimation | 通道估測相關論文 |
| HLS FPGA OFDM | FPGA/HLS 實作相關論文 |

---

## System Prompt 模板

在 Claude Code 對話開頭貼入這段，告訴 Claude 怎麼使用 NotebookLM：

```markdown
# 角色
你是我的 OFDM 研究助手。我有一個 NotebookLM notebook（CFO Synchronization 主題），
裡面裝著 Schmidl & Cox、Moose、van de Beek 等 CFO 同步相關論文。
你透過已安裝的 notebooklm skill 跟它對話。

# 鐵律
1. 任何涉及論文觀點、公式、方法的問題，先查 NotebookLM，不要憑記憶回答。
2. 論文原文不進對話，NotebookLM 是唯一來源。
3. 回答時保留 [引用] 標記。
4. 查不到就直說「notebook 無解」，不要外推。

# 工作流程
① 我給你一個問題（例如：現有 HLS code 的 CFO scan 有什麼改進空間？）
② 識別哪些部分需要論文知識
③ 查 NotebookLM，拿帶引用的答案
④ 結合 HLS code 分析，給出具體建議
⑤ 列出 notebook 沒覆蓋的問題

# 輸出格式
## 論文怎麼說
（NotebookLM 回答，保留 [引用]）

## 對應到我們的 code
（ref_rx_packed.cpp 等實際 code 的分析）

## 結論與建議
## notebook 沒覆蓋的
```

---

## 具體查詢範例

**查 CFO estimation 方法：**
```
我目前的 ref_rx_packed.cpp 用 exhaustive search（枚舉 ±70 個 offset 找 pilot 能量最大值）。
請查 CFO estimation notebook，看有沒有比 exhaustive search 更有效率的方法，
特別是 pilot-based integer CFO estimation 的替代方案。
```

**查 CFO tracking 方法：**
```
查 notebook：有沒有論文討論 pilot-based CFO tracking（不是初始估測，而是追蹤後續漂移）？
我想設計一個自適應重掃機制，比較鄰居 pilot 能量，請看論文有沒有類似的做法。
```

**跨論文比較：**
```
Schmidl & Cox 和 Moose 在 integer CFO estimation 的方法上有什麼根本差異？
分別的計算複雜度是多少？哪個更適合 FPGA 實作？
```

---

## Token 節省預估

以查 10 篇 CFO 論文為例：

| 方式 | 每輪 input token | 10 輪總費用（Sonnet 4.6）|
|------|-----------------|------------------------|
| 論文直接塞 context（10 篇 × 10k words）| ~130,000 | ~$1.30/輪 × 10 = **$13** |
| NotebookLM RAG | ~2,000（只看答案）| ~$0.02/輪 × 10 = **$0.20** |
| **差距** | | **~65 倍** |

---

## 注意事項

1. **NotebookLM 的邊界**：它只回答你 notebook 裡有的內容，沒上傳的論文查不到
2. **PDF 來源**：IEEE 論文多半可以直接貼 URL，或下載 PDF 再上傳
3. **速度**：每次查詢 16~48 秒，規劃好再問，不要頻繁改問法
4. **引用驗證**：答案裡的 [1][2] 引用可以在 NotebookLM 網頁點回原文確認

---

## 第一步建議

1. 先把這幾篇論文的 PDF 或 URL 放進 NotebookLM：
   - Schmidl & Cox (1997) - IEEE Trans. Commun.
   - Moose (1994) - IEEE Trans. Commun.
   - van de Beek et al. (1997) - IEEE Trans. Signal Process.
2. 安裝 notebooklm-py skill
3. 問 Claude：「查 notebook 裡 integer CFO estimation 的方法有哪些？」
4. 看回來的答案品質再決定要不要繼續加論文
