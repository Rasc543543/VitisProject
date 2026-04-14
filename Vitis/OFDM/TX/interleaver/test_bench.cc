//=============================================================================
// TESTBENCH (interleaver_testbench.cpp)
//
// Combines "Automatic Verification" and "Visual Output"
//=============================================================================

#include "interleaver.h" // HLS Core Header
#include <iostream>
#include <vector>    // Use vector for simplified data storage
#include <iomanip>   // For std::setw, std::hex
#include <algorithm> // For std::min

// === 1. HLS Core Function Prototype ===
void tx_interleaver(
    hls::stream<axis_word>& input_stream,
    hls::stream<axis_word>& output_stream
);

// === 2. Testbench Global Constants ===
const int DATA_LEN = 204;
const int COL_NUM = 16;
const int ROW_NUM = 12;
const int RESIDUE = 12;
const int TOTAL_ROWS = ROW_NUM + 1; // 13

// === 3. (New) Utility Function: Print Data ===
void print_data_sample(const std::vector<unsigned char>& data, const std::string& label, int count = DATA_LEN) {
    std::cout << "--- " << label << " ---" << std::endl;
    std::cout << "(" << data.size() << " bytes total, showing " << std::min(count, static_cast<int>(data.size())) << ")" << std::endl;
    
    for (int i = 0; i < data.size() && i < count; ++i) {
        // Display in Hex (Decimal) format
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i])
                  << " (" << std::dec << std::setw(3) << static_cast<int>(data[i]) << ") ";
        if ((i + 1) % 8 == 0) { // Newline every 8 bytes
            std::cout << std::endl;
        }
    }
    if (data.size() > 0 && (std::min(count, static_cast<int>(data.size())) % 8 != 0)) {
        std::cout << std::endl;
    }
    std::cout << std::endl; // Extra newline at the end
}


// === 4. Testbench Main Function (main) ===
int main() {
    
    // === 4.1 Prepare Inputs/Outputs ===
    hls::stream<axis_word> input_stream("input_stream");
    hls::stream<axis_word> output_stream("output_stream");

    // Use std::vector for data storage
    std::vector<unsigned char> input_data(DATA_LEN);
    std::vector<unsigned char> expected_output(DATA_LEN);
    std::vector<unsigned char> hls_output(DATA_LEN); // For storing HLS actual output

    // Prepare "Golden" Input Data (0, 1, 2, ..., 203)
    for (int i = 0; i < DATA_LEN; i++) {
        input_data[i] = (unsigned char)i;
    }

    // Prepare "Expected" Output Data (manual algorithm execution)
    int k = 0;
    for (int c = 0; c < COL_NUM; c++) {
        int rows_in_this_col = (c < RESIDUE) ? (TOTAL_ROWS) : (ROW_NUM);
        for (int r = 0; r < rows_in_this_col; r++) {
            int input_index = r * COL_NUM + c;
            expected_output[k] = input_data[input_index];
            k++;
        }
    }

    // === 4.2 Feed Input Data ===
    for (int i = 0; i < DATA_LEN; i++) {
        axis_word in_word;
        in_word.data = input_data[i];
        in_word.last = (i == DATA_LEN - 1) ? 1 : 0;
        in_word.keep = -1;
        in_word.strb = -1;
        input_stream.write(in_word);
    }

    // Print input and expected results
    std::cout << "========== Interleaver Testbench Start ==========" << std::endl;
    print_data_sample(input_data, "1. Input Data");
    print_data_sample(expected_output, "2. Expected Output");


    // === 4.3 Call the DUT (HLS Core) ===
    
    std::cout << "\n--- 3. Calling HLS Core (tx_interleaver) ---" << std::endl;
    tx_interleaver(input_stream, output_stream);
    std::cout << "--- HLS Core Execution Finished ---" << std::endl << std::endl;


    // === 4.4 Read HLS Output and Store ===
    int error_count = 0;
    bool tlast_error = false;
    
    for (int i = 0; i < DATA_LEN; i++) {
        if (output_stream.empty()) {
            std::cout << "!!! FATAL ERROR !!! HLS output stream empty. Expected " << DATA_LEN << " bytes, but stream was empty at index " << i << "." << std::endl;
            error_count = DATA_LEN; // Mark as fatal failure
            break;
        }
        
        axis_word out_word = output_stream.read();
        
        // Store HLS output for later printing
        hls_output[i] = out_word.data;

        // Automatic Verification Logic
        // 1. Compare Data
        if (out_word.data != expected_output[i] && error_count < 10) { // Only show first 10 errors
            if (error_count == 0) std::cout << "!!! Verification FAIL !!!" << std::endl;
            std::cout << "  - ERROR (Index " << i << "): "
                      << "HLS Output (Got) " << (int)out_word.data 
                      << ", Expected " << (int)expected_output[i] << std::endl;
            error_count++;
        }
        
        // 2. Compare TLAST
        bool expected_last = (i == DATA_LEN - 1) ? 1 : 0;
        if (out_word.last != expected_last) {
            tlast_error = true;
        }
    }
    
    if (tlast_error) {
        std::cout << "!!! TLAST Error !!! HLS output TLAST signal is incorrect." << std::endl;
        error_count++;
    }

    // Print HLS actual output
    print_data_sample(hls_output, "4. HLS Actual Output");


    // === 4.5 Display Final Result ===
    std::cout << "========== 5. Final Verification Result ==========" << std::endl;
    if (error_count == 0) {
        std::cout << "\n*** Test Passed (PASS)! ***" << std::endl;
        std::cout << "HLS core functionality matches testbench expectations." << std::endl;
    } else {
        std::cout << "\n*** Test Failed (FAIL)! ***" << std::endl;
        std::cout << "Found " << error_count << " data or TLAST errors." << std::endl;
    }

    return error_count; // Return 0 for PASS
}