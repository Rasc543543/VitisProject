// ============================================================
// tb_vivado.v — AXI-Stream Testbench Template
// 每次新專案需要修改的地方：
//   [1] DUT module name：把 "top" 換成你的 top function 名稱
//   [2] 串流 port prefix：input_stream / output_stream 要與 HLS 一致
//   [3] DATA_WIDTH：改成你的資料寬度（預設 8-bit）
//   [4] TEST_LEN 與 test_data：填入你的測試資料
// ============================================================
`timescale 1ns/1ps

module tb_vivado;

// ---- Parameters ----
parameter DATA_WIDTH = 8;
parameter CLK_PERIOD = 10;   // ns → 100 MHz
parameter TEST_LEN   = 8;    // [修改] 測試資料長度

// ---- Clock & Reset ----
reg ap_clk   = 0;
reg ap_rst_n = 0;

always #(CLK_PERIOD/2) ap_clk = ~ap_clk;

initial begin
    ap_rst_n = 0;
    repeat(10) @(posedge ap_clk);
    ap_rst_n = 1;
end

// ---- AXI-Stream Input (TB → DUT) ----
reg  [DATA_WIDTH-1:0] in_tdata  = 0;
reg                   in_tvalid = 0;
reg                   in_tlast  = 0;
wire                  in_tready;

// ---- AXI-Stream Output (DUT → TB) ----
wire [DATA_WIDTH-1:0] out_tdata;
wire                  out_tvalid;
wire                  out_tlast;
reg                   out_tready = 1;  // 預設永遠 ready

// ---- DUT 實例化 ----
// [修改 1] 把 "top" 換成你的 top function 名稱
// [修改 2] 確認 port 名稱與 HLS 產生的一致（可從 syn/verilog/ 的 .v 確認）
top dut (
    .ap_clk                  (ap_clk),
    .ap_rst_n                (ap_rst_n),
    // Input AXI-Stream
    .input_stream_TDATA      (in_tdata),
    .input_stream_TVALID     (in_tvalid),
    .input_stream_TREADY     (in_tready),
    .input_stream_TLAST      (in_tlast),
    .input_stream_TKEEP      ({(DATA_WIDTH/8){1'b1}}),
    .input_stream_TSTRB      ({(DATA_WIDTH/8){1'b1}}),
    // Output AXI-Stream
    .output_stream_TDATA     (out_tdata),
    .output_stream_TVALID    (out_tvalid),
    .output_stream_TREADY    (out_tready),
    .output_stream_TLAST     (out_tlast)
);

// ---- 接收監控 ----
integer rx_count = 0;
always @(posedge ap_clk) begin
    if (out_tvalid && out_tready) begin
        $display("[RX %0t] byte[%0d] = 0x%02X  last=%b",
                 $time, rx_count, out_tdata, out_tlast);
        rx_count = rx_count + 1;
    end
end

// ---- 測試主程式 ----
integer i;
reg [DATA_WIDTH-1:0] test_data [0:TEST_LEN-1];

initial begin
    // [修改] 填入測試資料
    test_data[0] = 8'hAA;
    test_data[1] = 8'hBB;
    test_data[2] = 8'hCC;
    test_data[3] = 8'hDD;
    test_data[4] = 8'hEE;
    test_data[5] = 8'hFF;
    test_data[6] = 8'h11;
    test_data[7] = 8'h22;

    // 等 reset 完成
    @(posedge ap_rst_n);
    repeat(5) @(posedge ap_clk);

    // 送資料進 DUT
    for (i = 0; i < TEST_LEN; i = i + 1) begin
        @(posedge ap_clk);
        in_tdata  = test_data[i];
        in_tvalid = 1;
        in_tlast  = (i == TEST_LEN - 1) ? 1 : 0;
        wait(in_tready == 1);
    end
    @(posedge ap_clk);
    in_tvalid = 0;
    in_tlast  = 0;

    // 等輸出
    repeat(2000) @(posedge ap_clk);
    $display("\n[DONE] Total received: %0d bytes", rx_count);
    $finish;
end

// ---- Timeout 保護 ----
initial begin
    #500000;
    $display("[TIMEOUT] Simulation exceeded time limit");
    $finish;
end

endmodule
