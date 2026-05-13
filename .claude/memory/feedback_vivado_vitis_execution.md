---
name: feedback-vivado-vitis-execution
description: Vivado/Vitis 流程（synthesis、simulation、implementation）由使用者自己動手，除非明確說「你跑」
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9bd136ea-15fd-4eb8-8c5d-218b78fca8ad
---

Vivado 或 Vitis 的任何流程（synthesis、simulation、implementation、export IP 等）都先由使用者自己動手，Claude 不主動執行。

**Why:** 使用者希望掌控工具執行時機，避免 Claude 在未確認的情況下啟動耗時或不可逆的操作。

**How to apply:** 準備好指令或腳本後，告知使用者步驟即可。等使用者明確說「你跑」或「讓你跑」才自行執行。
