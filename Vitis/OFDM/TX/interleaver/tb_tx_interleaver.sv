`timescale 1ns / 1ps

//////////////////////////////////////////////////////////////////////////////////
// Company: 
// Engineer: 
// 
// Create Date: 2025/11/17 00:10:30
// Design Name: tx_interleaver Testbench (Final Robust Version)
// Module Name: tb_tx_interleaver
// Project Name: 
// Target Devices: 
// Tool Versions: 
// Description: 
//   - 完整的自我檢查 (Self-Checking) Testbench
//   - 修復了 Deadlock 問題 (Receiver always block)
//   - 修復了 First Data Loss 問題 (Startup Delay)
//   - 使用 0 到 203 的輸入序列
//
//////////////////////////////////////////////////////////////////////////////////

module tb_tx_interleaver;

    // === 1. Testbench Parameters ===
    parameter CLK_PERIOD = 10;      // 10ns = 100MHz
    parameter DATA_LEN = 204;
    parameter COL_NUM = 16;
    parameter ROW_NUM = 12;
    parameter RESIDUE = 12;
    parameter TOTAL_ROWS = 13;

    // === 2. Signals (Regs and Wires) ===
    reg ap_clk_0;
    reg ap_rst_n_0;
    
    // Input AXI-Stream
    reg  [7:0] input_stream_0_tdata;
    reg  [0:0] input_stream_0_tkeep;
    reg  [0:0] input_stream_0_tlast;
    wire       input_stream_0_tready;
    reg  [0:0] input_stream_0_tstrb;
    reg        input_stream_0_tvalid;

    // Output AXI-Stream
    wire [7:0] output_stream_0_tdata;
    wire [0:0] output_stream_0_tkeep;
    wire [0:0] output_stream_0_tlast;
    reg        output_stream_0_tready; // 必須是 reg
    wire [0:0] output_stream_0_tstrb;
    wire       output_stream_0_tvalid;

    // === 3. Testbench Internal Storage ===
    reg [7:0] input_data[0:DATA_LEN-1];
    reg [7:0] expected_output[0:DATA_LEN-1];
    reg [7:0] hls_output[0:DATA_LEN-1];

    integer i, k, c, r, rows_in_this_col;
    reg [31:0] recv_idx; // 接收計數器
    integer error_count;

    // === 4. Instantiate the Device Under Test (DUT) ===
    tx_interleaver_sim_wrapper DUT (
        .ap_clk_0(ap_clk_0),
        .ap_rst_n_0(ap_rst_n_0),
        
        .input_stream_0_tdata(input_stream_0_tdata),
        .input_stream_0_tkeep(input_stream_0_tkeep),
        .input_stream_0_tlast(input_stream_0_tlast),
        .input_stream_0_tready(input_stream_0_tready),
        .input_stream_0_tstrb(input_stream_0_tstrb),
        .input_stream_0_tvalid(input_stream_0_tvalid),
        
        .output_stream_0_tdata(output_stream_0_tdata),
        .output_stream_0_tkeep(output_stream_0_tkeep),
        .output_stream_0_tlast(output_stream_0_tlast),
        .output_stream_0_tready(output_stream_0_tready),
        .output_stream_0_tstrb(output_stream_0_tstrb),
        .output_stream_0_tvalid(output_stream_0_tvalid)
    );

    // === 5. Clock Generator ===
    initial begin
        ap_clk_0 = 0;
        forever #(CLK_PERIOD/2) ap_clk_0 = ~ap_clk_0;
    end

    // === 6. Main Simulation Process (Sender and Verification) ===
    initial begin
        // --- 0. Prepare Data ---
        $display("========== Interleaver Testbench Start ==========");
        
        // 準備輸入資料 (0, 1, 2, ..., 203)
        $display("--- 0. Generating Input Data (0 to 203) ---");
        for (i = 0; i < DATA_LEN; i = i + 1) begin
            input_data[i] = i;
        end
        
        // 計算預期輸出 (黃金模型)
        k = 0;
        for (c = 0; c < COL_NUM; c = c + 1) begin
            rows_in_this_col = (c < RESIDUE) ? TOTAL_ROWS : ROW_NUM;
            for (r = 0; r < rows_in_this_col; r = r + 1) begin
                expected_output[k] = input_data[r * COL_NUM + c];
                k = k + 1;
            end
        end
        $display("--- 1. Input Data and Expected Data Generated ---");

        // --- 1. Reset Sequence ---
        ap_rst_n_0 = 1'b0; // 按下 Reset
        input_stream_0_tvalid = 1'b0;
        input_stream_0_tlast = 1'b0;
        output_stream_0_tready = 1'b0; // Reset 時 TREADY 為低
        repeat(10) @(posedge ap_clk_0);
        
        ap_rst_n_0 = 1'b1; // 放開 Reset
        output_stream_0_tready = 1'b1; // (重要!) 永遠準備好接收
        
        // [!code focus] === 啟動延遲 (修復第一筆資料遺失) ===
        $display("--- Waiting for HLS Core to initialize... ---");
        repeat(50) @(posedge ap_clk_0); 
        // ===============================================

        @(posedge ap_clk_0);
        $display("--- 2. Reset Finished (output_tready is NOW HIGH) ---");

        // --- 2. Sender Task ---
        $display("--- 3. Calling HLS Core (Sending Data) ---");
        for (i = 0; i < DATA_LEN; i = i + 1) begin
            input_stream_0_tvalid = 1'b1;
            input_stream_0_tdata = input_data[i];
            input_stream_0_tkeep = 1'b1;
            input_stream_0_tstrb = 1'b1;
            input_stream_0_tlast = (i == DATA_LEN - 1) ? 1'b1 : 1'b0;
            
            // (穩健的 AXI-Stream 握手)
            // 等待 HLS 核心說 Ready
            @(posedge ap_clk_0);
            while (input_stream_0_tready == 1'b0) begin
                @(posedge ap_clk_0);
            end
            // 傳輸在此時脈邊緣完成
        end
        
        // 傳送結束
        @(posedge ap_clk_0);
        input_stream_0_tvalid = 1'b0;
        input_stream_0_tlast = 1'b0;
        $display("--- HLS Core Send Finished ---");

        // --- 3. Wait for Receiver to Finish ---
        // 等待接收器 (always 區塊) 收到 204 筆資料
        $display("--- Waiting for HLS Core Receive to Finish... ---");
        wait(recv_idx == DATA_LEN);
        @(posedge ap_clk_0);
        $display("--- HLS Core Receive Finished ---");

        // --- 4. Final Verification ---
        repeat(5) @(posedge ap_clk_0); // 緩衝
        $display("--- 4. HLS Actual Output (Verification) ---");
        error_count = 0;
        for (i = 0; i < DATA_LEN; i = i + 1) begin
            if (hls_output[i] != expected_output[i]) begin
                if (error_count < 10) begin
                    if (error_count == 0) $display("!!! Verification FAIL !!!");
                    $display("  - ERROR (Index %0d): HLS Output (Got) 0x%h, Expected 0x%h", i, hls_output[i], expected_output[i]);
                end
                error_count = error_count + 1;
            end
        end

        // --- 5. Final Verification Result ---
        $display("========== 5. Final Verification Result ==========");
        if (error_count == 0) begin
            $display("\n*** Test Passed (PASS)! ***");
            $display("HLS core functionality matches testbench expectations.");
        end else begin
            $display("\n*** Test Failed (FAIL)! ***");
            $display("Found %0d data errors.", error_count);
        end

        // 結束模擬
        $finish;
    end


    // === 7. Receiver Process (Always-on) ===
    // 獨立的接收器，負責監聽輸出
    initial begin
        recv_idx = 0; 
    end

    always @(posedge ap_clk_0) begin
        if (ap_rst_n_0 == 1'b0) begin
            recv_idx <= 0;
        end else begin
            // 檢查有效傳輸 (Valid=1 且 Ready=1)
            if (output_stream_0_tvalid == 1'b1 && output_stream_0_tready == 1'b1) begin
                
                if (recv_idx < DATA_LEN) begin
                    // 存入陣列
                    hls_output[recv_idx] <= output_stream_0_tdata;
                    
                    // 更新計數器
                    if (output_stream_0_tlast == 1'b1) begin
                        if (recv_idx != DATA_LEN - 1) begin
                            $display("!!! TLAST Error !!! Received TLAST at index %d, expected %d", recv_idx, DATA_LEN - 1);
                        end
                        recv_idx <= DATA_LEN; 
                    end else begin
                        recv_idx <= recv_idx + 1;
                    end
                end
            end
        end
    end

endmodule