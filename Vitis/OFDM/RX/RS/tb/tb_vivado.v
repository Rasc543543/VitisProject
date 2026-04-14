// ============================================================
// tb_vivado.v — RS Decoder RX  AXI-Stream Testbench
// 使用 TX golden pattern 驗證 RS RX 演算法正確性
// QPSK: 送入 204 bytes codeword，驗證輸出 = 原始 188 bytes 輸入
// ============================================================
`timescale 1ns/1ps

module tb_vivado;

// ---- Parameters ----
parameter CLK_PERIOD = 10;    // ns → 100 MHz
parameter N_QPSK     = 204;   // RS(204,188) codeword 長度
parameter K_QPSK     = 188;   // RS(204,188) 資料長度

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
reg  [7:0] in_tdata  = 0;
reg  [0:0] in_tkeep  = 1;
reg  [0:0] in_tstrb  = 1;
reg        in_tvalid = 0;
reg  [0:0] in_tlast  = 0;
wire       in_tready;

// ---- AXI-Stream Output (DUT → TB) ----
wire [7:0] out_tdata;
wire [0:0] out_tkeep;
wire [0:0] out_tstrb;
wire       out_tvalid;
wire [0:0] out_tlast;
reg        out_tready = 1;

// ---- DUT 實例化（對應 design_1_wrapper）----
design_1_wrapper dut (
    .ap_clk_0                (ap_clk),
    .ap_rst_n_0              (ap_rst_n),
    .input_stream_0_tdata    (in_tdata),
    .input_stream_0_tkeep    (in_tkeep),
    .input_stream_0_tstrb    (in_tstrb),
    .input_stream_0_tvalid   (in_tvalid),
    .input_stream_0_tlast    (in_tlast),
    .input_stream_0_tready   (in_tready),
    .output_stream_0_tdata   (out_tdata),
    .output_stream_0_tkeep   (out_tkeep),
    .output_stream_0_tstrb   (out_tstrb),
    .output_stream_0_tvalid  (out_tvalid),
    .output_stream_0_tlast   (out_tlast),
    .output_stream_0_tready  (out_tready)
);

// ---- Golden QPSK codeword (204 bytes, from TX RS csim) ----
// Input to TX: data[0]=0x47, data[i]=i&0xFF
// Output of TX (= input to RX): 188 data bytes + 16 parity bytes
reg [7:0] codeword [0:N_QPSK-1];
reg [7:0] expected [0:K_QPSK-1];

integer i;
initial begin
    // codeword = data (188 bytes) + parity (16 bytes)
    codeword[  0] = 8'h47; codeword[  1] = 8'h01; codeword[  2] = 8'h02; codeword[  3] = 8'h03;
    codeword[  4] = 8'h04; codeword[  5] = 8'h05; codeword[  6] = 8'h06; codeword[  7] = 8'h07;
    codeword[  8] = 8'h08; codeword[  9] = 8'h09; codeword[ 10] = 8'h0A; codeword[ 11] = 8'h0B;
    codeword[ 12] = 8'h0C; codeword[ 13] = 8'h0D; codeword[ 14] = 8'h0E; codeword[ 15] = 8'h0F;
    codeword[ 16] = 8'h10; codeword[ 17] = 8'h11; codeword[ 18] = 8'h12; codeword[ 19] = 8'h13;
    codeword[ 20] = 8'h14; codeword[ 21] = 8'h15; codeword[ 22] = 8'h16; codeword[ 23] = 8'h17;
    codeword[ 24] = 8'h18; codeword[ 25] = 8'h19; codeword[ 26] = 8'h1A; codeword[ 27] = 8'h1B;
    codeword[ 28] = 8'h1C; codeword[ 29] = 8'h1D; codeword[ 30] = 8'h1E; codeword[ 31] = 8'h1F;
    codeword[ 32] = 8'h20; codeword[ 33] = 8'h21; codeword[ 34] = 8'h22; codeword[ 35] = 8'h23;
    codeword[ 36] = 8'h24; codeword[ 37] = 8'h25; codeword[ 38] = 8'h26; codeword[ 39] = 8'h27;
    codeword[ 40] = 8'h28; codeword[ 41] = 8'h29; codeword[ 42] = 8'h2A; codeword[ 43] = 8'h2B;
    codeword[ 44] = 8'h2C; codeword[ 45] = 8'h2D; codeword[ 46] = 8'h2E; codeword[ 47] = 8'h2F;
    codeword[ 48] = 8'h30; codeword[ 49] = 8'h31; codeword[ 50] = 8'h32; codeword[ 51] = 8'h33;
    codeword[ 52] = 8'h34; codeword[ 53] = 8'h35; codeword[ 54] = 8'h36; codeword[ 55] = 8'h37;
    codeword[ 56] = 8'h38; codeword[ 57] = 8'h39; codeword[ 58] = 8'h3A; codeword[ 59] = 8'h3B;
    codeword[ 60] = 8'h3C; codeword[ 61] = 8'h3D; codeword[ 62] = 8'h3E; codeword[ 63] = 8'h3F;
    codeword[ 64] = 8'h40; codeword[ 65] = 8'h41; codeword[ 66] = 8'h42; codeword[ 67] = 8'h43;
    codeword[ 68] = 8'h44; codeword[ 69] = 8'h45; codeword[ 70] = 8'h46; codeword[ 71] = 8'h47;
    codeword[ 72] = 8'h48; codeword[ 73] = 8'h49; codeword[ 74] = 8'h4A; codeword[ 75] = 8'h4B;
    codeword[ 76] = 8'h4C; codeword[ 77] = 8'h4D; codeword[ 78] = 8'h4E; codeword[ 79] = 8'h4F;
    codeword[ 80] = 8'h50; codeword[ 81] = 8'h51; codeword[ 82] = 8'h52; codeword[ 83] = 8'h53;
    codeword[ 84] = 8'h54; codeword[ 85] = 8'h55; codeword[ 86] = 8'h56; codeword[ 87] = 8'h57;
    codeword[ 88] = 8'h58; codeword[ 89] = 8'h59; codeword[ 90] = 8'h5A; codeword[ 91] = 8'h5B;
    codeword[ 92] = 8'h5C; codeword[ 93] = 8'h5D; codeword[ 94] = 8'h5E; codeword[ 95] = 8'h5F;
    codeword[ 96] = 8'h60; codeword[ 97] = 8'h61; codeword[ 98] = 8'h62; codeword[ 99] = 8'h63;
    codeword[100] = 8'h64; codeword[101] = 8'h65; codeword[102] = 8'h66; codeword[103] = 8'h67;
    codeword[104] = 8'h68; codeword[105] = 8'h69; codeword[106] = 8'h6A; codeword[107] = 8'h6B;
    codeword[108] = 8'h6C; codeword[109] = 8'h6D; codeword[110] = 8'h6E; codeword[111] = 8'h6F;
    codeword[112] = 8'h70; codeword[113] = 8'h71; codeword[114] = 8'h72; codeword[115] = 8'h73;
    codeword[116] = 8'h74; codeword[117] = 8'h75; codeword[118] = 8'h76; codeword[119] = 8'h77;
    codeword[120] = 8'h78; codeword[121] = 8'h79; codeword[122] = 8'h7A; codeword[123] = 8'h7B;
    codeword[124] = 8'h7C; codeword[125] = 8'h7D; codeword[126] = 8'h7E; codeword[127] = 8'h7F;
    codeword[128] = 8'h80; codeword[129] = 8'h81; codeword[130] = 8'h82; codeword[131] = 8'h83;
    codeword[132] = 8'h84; codeword[133] = 8'h85; codeword[134] = 8'h86; codeword[135] = 8'h87;
    codeword[136] = 8'h88; codeword[137] = 8'h89; codeword[138] = 8'h8A; codeword[139] = 8'h8B;
    codeword[140] = 8'h8C; codeword[141] = 8'h8D; codeword[142] = 8'h8E; codeword[143] = 8'h8F;
    codeword[144] = 8'h90; codeword[145] = 8'h91; codeword[146] = 8'h92; codeword[147] = 8'h93;
    codeword[148] = 8'h94; codeword[149] = 8'h95; codeword[150] = 8'h96; codeword[151] = 8'h97;
    codeword[152] = 8'h98; codeword[153] = 8'h99; codeword[154] = 8'h9A; codeword[155] = 8'h9B;
    codeword[156] = 8'h9C; codeword[157] = 8'h9D; codeword[158] = 8'h9E; codeword[159] = 8'h9F;
    codeword[160] = 8'hA0; codeword[161] = 8'hA1; codeword[162] = 8'hA2; codeword[163] = 8'hA3;
    codeword[164] = 8'hA4; codeword[165] = 8'hA5; codeword[166] = 8'hA6; codeword[167] = 8'hA7;
    codeword[168] = 8'hA8; codeword[169] = 8'hA9; codeword[170] = 8'hAA; codeword[171] = 8'hAB;
    codeword[172] = 8'hAC; codeword[173] = 8'hAD; codeword[174] = 8'hAE; codeword[175] = 8'hAF;
    codeword[176] = 8'hB0; codeword[177] = 8'hB1; codeword[178] = 8'hB2; codeword[179] = 8'hB3;
    codeword[180] = 8'hB4; codeword[181] = 8'hB5; codeword[182] = 8'hB6; codeword[183] = 8'hB7;
    codeword[184] = 8'hB8; codeword[185] = 8'hB9; codeword[186] = 8'hBA; codeword[187] = 8'hBB;
    // parity (16 bytes)
    codeword[188] = 8'h4F; codeword[189] = 8'h29; codeword[190] = 8'hDC; codeword[191] = 8'h45;
    codeword[192] = 8'h0E; codeword[193] = 8'h4C; codeword[194] = 8'h03; codeword[195] = 8'h5B;
    codeword[196] = 8'hBA; codeword[197] = 8'hE8; codeword[198] = 8'h93; codeword[199] = 8'h84;
    codeword[200] = 8'h03; codeword[201] = 8'h00; codeword[202] = 8'hE0; codeword[203] = 8'h04;

    // expected output = original TX input (188 bytes)
    expected[  0] = 8'h47;
    for (i = 1; i < K_QPSK; i = i + 1)
        expected[i] = i[7:0];
end

// ---- 接收與驗證 ----
integer rx_count  = 0;
integer rx_errors = 0;

always @(posedge ap_clk) begin
    if (out_tvalid && out_tready) begin
        if (rx_count < K_QPSK) begin
            if (out_tdata !== expected[rx_count]) begin
                $display("[MISMATCH] byte[%0d]: got 0x%02X, expected 0x%02X",
                         rx_count, out_tdata, expected[rx_count]);
                rx_errors = rx_errors + 1;
            end else if (rx_count < 8 || out_tlast) begin
                $display("[RX %0t ns] byte[%0d] = 0x%02X  OK  last=%b",
                         $time, rx_count, out_tdata, out_tlast);
            end
        end
        rx_count = rx_count + 1;
    end
end

// ---- 主流程 ----
initial begin
    @(posedge ap_rst_n);
    repeat(5) @(posedge ap_clk);

    $display("\n[TX] Sending QPSK golden codeword (%0d bytes)...", N_QPSK);

    for (i = 0; i < N_QPSK; i = i + 1) begin
        @(posedge ap_clk);
        in_tdata  = codeword[i];
        in_tvalid = 1;
        in_tlast  = (i == N_QPSK - 1) ? 1 : 0;
        wait(in_tready == 1);
    end
    @(posedge ap_clk);
    in_tvalid = 0;
    in_tlast  = 0;

    $display("[TX] Done. Waiting for decode...");

    repeat(100000) @(posedge ap_clk);

    $display("\n[RESULT] Received: %0d / %0d bytes", rx_count, K_QPSK);
    if (rx_errors == 0 && rx_count == K_QPSK)
        $display("[PASS] Output matches TX input perfectly.");
    else
        $display("[FAIL] %0d mismatches, %0d bytes received.", rx_errors, rx_count);

    $finish;
end

// ---- Timeout ----
initial begin
    #2000000000;
    $display("[TIMEOUT]");
    $finish;
end

endmodule
