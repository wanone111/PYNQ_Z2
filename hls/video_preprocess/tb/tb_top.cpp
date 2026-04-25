#include "top.h"
#include <iostream>
#include <fstream>
#include <cstring>

// ============================================================================
// Testbench for Unified Video Preprocessing IP
// Tests: ISP + Dynamic ROI + RGB2YUV conversion
// ============================================================================

int main() {
    std::cout << "=== Video Preprocess HLS Testbench ===" << std::endl;
    std::cout << "Testing: ISP (WB+Gamma+Denoise) -> Dynamic ROI -> RGB2YUV" << std::endl;

    // Create input/output streams
    hls::stream<axi_rgb_t> src_stream("src_stream");
    hls::stream<axi_u8_t> y_dst_stream("y_dst_stream");
    hls::stream<axi_u8_t> uv_dst_stream("uv_dst_stream");

    // Test parameters
    const int TEST_ROWS = 480;
    const int TEST_COLS = 640;
    
    // ROI parameters (center region)
    const int ROI_X = 160;
    const int ROI_Y = 120;
    const int ROI_W = 320;
    const int ROI_H = 240;

    // ISP parameters
    const int WB_R_GAIN = 256;   // 1.0x (no change)
    const int WB_G_GAIN = 256;   // 1.0x
    const int WB_B_GAIN = 256;   // 1.0x
    const int GAMMA_MODE = GAMMA_MODE_BYPASS;
    const int DENOISE_STRENGTH = 0;
    const int GRAY_EN = 0;
    const int THRESHOLD_EN = 0;
    const int THRESHOLD_VAL = 128;
    const int INPUT_IS_BGR = 0;  // RGB input

    // Generate test input pattern (gradient)
    std::cout << "\nGenerating test input pattern (" << TEST_ROWS << "x" << TEST_COLS << ")..." << std::endl;
    for (int r = 0; r < TEST_ROWS; ++r) {
        for (int c = 0; c < TEST_COLS; ++c) {
            axi_rgb_t pix;
            // Create gradient pattern: R increases horizontally, B increases vertically
            pix.data.range(23, 16) = (c * 255 / TEST_COLS) & 0xFF;  // R
            pix.data.range(15, 8) = 128;                              // G
            pix.data.range(7, 0) = (r * 255 / TEST_ROWS) & 0xFF;    // B
            pix.keep = 1;
            pix.strb = 1;
            pix.user = (r == 0 && c == 0) ? 1 : 0;
            pix.last = (c == TEST_COLS - 1) ? 1 : 0;
            pix.id = 0;
            pix.dest = 0;
            src_stream.write(pix);
        }
    }

    std::cout << "Running video_preprocess kernel..." << std::endl;

    // Call the DUT
    video_preprocess(
        src_stream,
        y_dst_stream,
        uv_dst_stream,
        TEST_ROWS,
        TEST_COLS,
        ROI_X,
        ROI_Y,
        ROI_W,
        ROI_H,
        WB_R_GAIN,
        WB_G_GAIN,
        WB_B_GAIN,
        GAMMA_MODE,
        DENOISE_STRENGTH,
        GRAY_EN,
        THRESHOLD_EN,
        THRESHOLD_VAL,
        INPUT_IS_BGR
    );

    // Verify output
    std::cout << "\nVerifying Y output stream..." << std::endl;
    int y_count = 0;
    int y_errors = 0;
    bool first_y = true;
    
    while (!y_dst_stream.empty()) {
        axi_u8_t y_pkt = y_dst_stream.read();
        
        if (first_y) {
            if (y_pkt.user != 1) {
                std::cerr << "ERROR: First Y packet should have user=1" << std::endl;
                y_errors++;
            }
            first_y = false;
        }
        
        y_count++;
        
        if (y_pkt.last && y_count != TEST_COLS) {
            // Check row boundary
            std::cerr << "Warning: Y row length mismatch" << std::endl;
        }
    }

    std::cout << "Total Y packets: " << y_count << " (expected: " << TEST_ROWS * TEST_COLS << ")" << std::endl;
    if (y_count != TEST_ROWS * TEST_COLS) {
        std::cerr << "ERROR: Y packet count mismatch!" << std::endl;
        y_errors++;
    }

    // Verify UV output
    std::cout << "\nVerifying UV output stream..." << std::endl;
    int uv_count = 0;
    int uv_errors = 0;
    bool first_uv = true;
    
    while (!uv_dst_stream.empty()) {
        axi_u8_t uv_pkt = uv_dst_stream.read();
        
        if (first_uv) {
            if (uv_pkt.user != 1) {
                std::cerr << "ERROR: First UV packet should have user=1" << std::endl;
                uv_errors++;
            }
            first_uv = false;
        }
        
        uv_count++;
    }

    // Expected UV count: (rows/2) * cols (U and V interleaved)
    int expected_uv = (TEST_ROWS / 2) * TEST_COLS;
    std::cout << "Total UV packets: " << uv_count << " (expected: " << expected_uv << ")" << std::endl;
    if (uv_count != expected_uv) {
        std::cerr << "ERROR: UV packet count mismatch!" << std::endl;
        uv_errors++;
    }

    // Summary
    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Y stream errors: " << y_errors << std::endl;
    std::cout << "UV stream errors: " << uv_errors << std::endl;
    
    if (y_errors == 0 && uv_errors == 0) {
        std::cout << "TEST PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "TEST FAILED!" << std::endl;
        return 1;
    }
}
