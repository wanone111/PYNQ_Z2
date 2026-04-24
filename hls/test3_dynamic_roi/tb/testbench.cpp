#include <iostream>
#include <vector>
#include <cstdint>
#include <iomanip>
#include "top.h"
#include "gamma_lut.h"

static inline uint32_t pack_rgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline void unpack_rgb_u32(uint32_t v, uint8_t& r, uint8_t& g, uint8_t& b) {
    r = (v >> 16) & 0xFF;
    g = (v >> 8)  & 0xFF;
    b = v & 0xFF;
}

static inline uint8_t sat_u8_int(int x) {
    if (x < 0) return 0;
    if (x > 255) return 255;
    return (uint8_t)x;
}

static inline uint8_t wb_gain_ref(uint8_t pix, unsigned gain_q4_8) {
    unsigned mult = (unsigned)pix * gain_q4_8 + 128;
    unsigned res  = mult >> 8;
    return (res > 255) ? 255 : (uint8_t)res;
}

static uint32_t reference_pixel(
    uint32_t cur_in,
    bool left_valid,
    uint32_t left_gamma,
    bool up_valid,
    uint32_t up_gamma,
    unsigned wb_r_gain,
    unsigned wb_g_gain,
    unsigned wb_b_gain,
    unsigned gamma_sel
) {
    uint8_t r, g, b;
    unpack_rgb_u32(cur_in, r, g, b);

    uint8_t r_wb = wb_gain_ref(r, wb_r_gain);
    uint8_t g_wb = wb_gain_ref(g, wb_g_gain);
    uint8_t b_wb = wb_gain_ref(b, wb_b_gain);

    uint8_t r_gm = (uint8_t)gamma_lut_r[gamma_sel][r_wb];
    uint8_t g_gm = (uint8_t)gamma_lut_g[gamma_sel][g_wb];
    uint8_t b_gm = (uint8_t)gamma_lut_b[gamma_sel][b_wb];
    uint32_t cur_gamma = pack_rgb_u32(r_gm, g_gm, b_gm);

    uint8_t cr, cg, cb;
    uint8_t lr, lg, lb;
    uint8_t ur, ug, ub;
    unpack_rgb_u32(cur_gamma, cr, cg, cb);
    unpack_rgb_u32(left_gamma, lr, lg, lb);
    unpack_rgb_u32(up_gamma,   ur, ug, ub);

    if (!left_valid) {
        lr = cr; lg = cg; lb = cb;
    }
    if (!up_valid) {
        ur = cr; ug = cg; ub = cb;
    }

    uint8_t r_dn = (uint8_t)(((cr << 2) + (lr << 1) + (ur << 1)) >> 3);
    uint8_t g_dn = (uint8_t)(((cg << 2) + (lg << 1) + (ug << 1)) >> 3);
    uint8_t b_dn = (uint8_t)(((cb << 2) + (lb << 1) + (ub << 1)) >> 3);

    return pack_rgb_u32(r_dn, g_dn, b_dn);
}

int main() {
    const int rows = 8;
    const int cols = 16;

    const int roi_x = 4;
    const int roi_y = 2;
    const int roi_w = 7;
    const int roi_h = 4;

    const ap_uint<12> wb_r_gain = 320; // 1.25
    const ap_uint<12> wb_g_gain = 256; // 1.00
    const ap_uint<12> wb_b_gain = 224; // 0.875
    const ap_uint<2>  gamma_sel = 2;   // gamma = 1.2

    hls::stream<axis_rgb_t> src;
    hls::stream<axis_rgb_t> dst;

    std::vector<uint32_t> in_frame(rows * cols, 0);
    std::vector<uint32_t> ref_frame(rows * cols, 0);

    // 构造测试图：RGB 梯度
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            uint8_t r = (uint8_t)((x * 13 + y * 7) & 0xFF);
            uint8_t g = (uint8_t)((x * 9  + y * 11) & 0xFF);
            uint8_t b = (uint8_t)((x * 5  + y * 17) & 0xFF);

            in_frame[y * cols + x] = pack_rgb_u32(r, g, b);

            axis_rgb_t pix;
            pix.data = in_frame[y * cols + x];
            pix.keep = -1;
            pix.strb = -1;
            pix.user = (x == 0 && y == 0) ? 1 : 0;
            pix.last = (x == cols - 1) ? 1 : 0;
            pix.id   = 0;
            pix.dest = 0;
            src.write(pix);
        }
    }

    // 参考模型
    std::vector<uint32_t> prev_row_gamma(cols, 0);
    std::vector<uint8_t>  prev_row_valid(cols, 0);

    for (int y = 0; y < rows; ++y) {
        uint32_t left_gamma = 0;
        bool left_valid = false;

        for (int x = 0; x < cols; ++x) {
            bool in_roi =
                (x >= roi_x) &&
                (x <  roi_x + roi_w) &&
                (y >= roi_y) &&
                (y <  roi_y + roi_h);

            uint32_t out = 0;

            if (in_roi) {
                uint32_t cur_in = in_frame[y * cols + x];
                uint32_t up_gamma = prev_row_gamma[x];
                bool up_valid = ((y > roi_y) && (prev_row_valid[x] != 0));

                out = reference_pixel(
                    cur_in,
                    left_valid, left_gamma,
                    up_valid, up_gamma,
                    (unsigned)wb_r_gain,
                    (unsigned)wb_g_gain,
                    (unsigned)wb_b_gain,
                    (unsigned)gamma_sel
                );

                // 更新 left / prev row 用 gamma_out
                uint8_t r, g, b;
                unpack_rgb_u32(cur_in, r, g, b);
                uint8_t r_wb = wb_gain_ref(r, (unsigned)wb_r_gain);
                uint8_t g_wb = wb_gain_ref(g, (unsigned)wb_g_gain);
                uint8_t b_wb = wb_gain_ref(b, (unsigned)wb_b_gain);
                uint8_t r_gm = (uint8_t)gamma_lut_r[(unsigned)gamma_sel][r_wb];
                uint8_t g_gm = (uint8_t)gamma_lut_g[(unsigned)gamma_sel][g_wb];
                uint8_t b_gm = (uint8_t)gamma_lut_b[(unsigned)gamma_sel][b_wb];
                uint32_t gamma_out = pack_rgb_u32(r_gm, g_gm, b_gm);

                left_gamma = gamma_out;
                left_valid = true;
                prev_row_gamma[x] = gamma_out;
                prev_row_valid[x] = 1;
            } else {
                out = 0;
                left_gamma = 0;
                left_valid = false;
                prev_row_gamma[x] = 0;
                prev_row_valid[x] = 0;
            }

            ref_frame[y * cols + x] = out;
        }
    }

    video_preprocess(
        src, dst,
        rows, cols,
        roi_x, roi_y, roi_w, roi_h,
        wb_r_gain, wb_g_gain, wb_b_gain,
        gamma_sel
    );

    int err = 0;
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            axis_rgb_t out_pix = dst.read();
            uint32_t got = (uint32_t)out_pix.data;
            uint32_t exp = ref_frame[y * cols + x];

            if (got != exp) {
                if (err < 16) {
                    std::cout << "Mismatch @ (" << y << "," << x << ") "
                              << "got=0x" << std::hex << std::setw(6) << std::setfill('0') << got
                              << " exp=0x" << std::setw(6) << exp << std::dec << "\n";
                }
                err++;
            }

            // sideband 基本检查
            if ((x == 0 && y == 0) && out_pix.user != 1) {
                std::cout << "ERROR: user not asserted on first pixel.\n";
                err++;
            }
            if ((x == cols - 1) && out_pix.last != 1) {
                std::cout << "ERROR: last not asserted at line end.\n";
                err++;
            }
            if ((x != cols - 1) && out_pix.last != 0) {
                std::cout << "ERROR: last asserted unexpectedly.\n";
                err++;
            }
        }
    }

    if (err == 0) {
        std::cout << "TB PASS: ROI + WB + Gamma + Denoise pipeline matched reference.\n";
        return 0;
    } else {
        std::cout << "TB FAIL: total mismatches = " << err << "\n";
        return 1;
    }
}
