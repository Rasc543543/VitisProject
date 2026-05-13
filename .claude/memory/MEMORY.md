# Memory Index

- [使用者基本資料](user_profile.md) — 碩一下研究生，有 C/Vitis 經驗，這台電腦專做 Vitis HLS 自動化
- [Vitis HLS 開發環境](vscode_environment.md) — C:\VitisProject，Vitis 2022.2，模板結構與 VS Code 設定
- [Vitis HLS 完整開發流程](WORKFLOW.md) — 新建專案步驟、tasks.json 任務說明、路徑問題、常見錯誤對照表
- [Vitis 上板應用程式碼](project_vitis_board.md) — test_loop/test_Brian，完整系統 274B，BER=0，HW=Sim 完全一致
- [Viterbi 隔離測試計畫](project_viterbi_isolation_test.md) — ✅完成：solo_test，新 Viterbi IP WNS=+1.068ns，HW BER=0
- [Vivado TB 腳位對應規則](feedback_vivado_tb.md) — TB 要對 design_1_wrapper，腳位加 _0 suffix，全小寫
- [Token 使用效率要求](feedback_token_efficiency.md) — 思考與回應都要精簡，不冗餘，結論先說
- [已解決問題不重複報告](feedback_no_reopen_solved.md) — 看程式碼確認狀態，不把舊分析文件當待辦事項
- [結論觸發記憶更新規則](feedback_memory_update_trigger.md) — 得出確定性結論時立即更新記憶，未結論時不動
- [Git 版本控制工作流程](feedback_git_workflow.md) — C:/VitisProject 已有 git，合成產物排除，改完測試後 commit
- [OFDM 通訊系統架構知識](project_ofdm_system.md) — 8PSK/QPSK 完整鏈路、IP 數量、RS/Viterbi 參數、TB 路徑
- [IP Clock 設定記錄](reference_ip_clock.md) — test_Brian chain 各 IP 的 HLS clock 設定（250/100 MHz），文檔在 C:\VitisProject\ip_clock_settings.md
- [Vivado 專案認識流程](workflow_vivado_project_exploration.md) — 拿到新 Vivado 專案的 SOP：BD→RTL→HLS→Log→TB
- [盲模式偵測設計討論](project_blind_mode_detection.md) — QPSK/8PSK 自動識別，DDR平行雙鏈路方案，文檔在 C:\VitisProject\design_notes\blind_mode_detection.md
- [Data Rate 優化計畫](project_datarate_optimization.md) — ✅CFO Tracking Mode 全部完成：QPSK HW 2.30 Mbps / 8PSK HW 2.95 Mbps，BER=0；待驗 argmax-at-edge 假設 + Doppler 計算
- [NotebookLM 安裝與 CFO 論文 Notebook](reference_notebooklm.md) — notebooklm-py 已裝好，CFO notebook ID=4c3a7ef2，五篇精選論文已探查，分析文件在 design_notes/cfo_papers_deep_dive
- [RS Encoder/Decoder 開發紀錄](project_rs_progress.md) — ✅ 全部完成（csim/cosim/Vivado PASS），golden pattern 與參數存檔
- [Vivado/Vitis 執行權限](feedback_vivado_vitis_execution.md) — synthesis/simulation/implementation 先由使用者動手，Claude 等「讓你跑」才執行
