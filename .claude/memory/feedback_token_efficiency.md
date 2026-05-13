---
name: Token 使用效率要求
description: 使用者 token 有限，要求思考與回應都要精簡高效
type: feedback
---

思考和回應都要以最節省 token 的方式進行。

**Why:** 使用者 token 有限，希望每個 token 都花在刀口上，不要浪費在冗餘推理或重複確認。

**How to apply:**
- 思考：直接跳到關鍵判斷，不重述已知資訊
- 回應：結論先說，省略鋪墊，不重述使用者說過的話
- 程式碼：只改必要部分，不順便重構
- 不在結尾加摘要列表

**開發分工（省 token 流程）：**
- Claude 負責：寫/修 HLS 主程式、寫/修 TB、分析錯誤並修正
- 使用者負責：執行 csim / syn / cosim / Vivado sim、看波型
- 失敗時：使用者說「csim fail」→ Claude 自己去 grep 對應 log 的 ERROR 行
- 成功時：使用者說「過了，繼續」→ Claude 寫下一個檔案

**各階段錯誤 log 位置（Vitis HLS）：**
- csim：`<proj>/solution1/csim/report/<top>_csim.log`
- syn：`<proj>/solution1/syn/report/<top>_csynth.rpt`
- cosim：`<proj>/solution1/sim/report/<top>_cosim.rpt`
- Vivado：請使用者告知專案路徑，Claude 自己找 .log
