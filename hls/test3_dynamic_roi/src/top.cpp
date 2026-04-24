#include "top.h"
#include "gamma_lut.h"

static inline ap_uint<8> sat_u8_from_u12(ap_uint<12> x) {
#pragma HLS INLINE
    return (x > 255) ? (ap_uint<8>)255 : (ap_uint<8>)x;
}

static inline ap_uint<8> wb_gain_u8(ap_uint<8> pix, ap_uint<12> gain_q4_8) {
#pragma HLS INLINE
    ap_uint<20> mult = (ap_uint<20>)pix * (ap_uint<20>)gain_q4_8 + 128;
    ap_uint<12> res  = (ap_uint<12>)(mult >> 8);
    return sat_u8_from_u12(res);
}

static inline void unpack_rgb(ap_uint<24> data, ap_uint<8>& r, ap_uint<8>& g, ap_uint<8>& b) {
#pragma HLS INLINE
    r = data.range(23, 16);
    g = data.range(15, 8);
    b = data.range(7, 0);
}

static inline ap_uint<24> pack_rgb(ap_uint<8> r, ap_uint<8> g, ap_uint<8> b) {
#pragma HLS INLINE
    ap_uint<24> out;
    out.range(23, 16) = r;
    out.range(15, 8)  = g;
    out.range(7, 0)   = b;
    return out;
}

static inline ap_uint<8> gamma_lookup_r(ap_uint<8> x, ap_uint<2> sel) {
#pragma HLS INLINE
    return gamma_lut_r[(unsigned)sel][(unsigned)x];
}

static inline ap_uint<8> gamma_lookup_g(ap_uint<8> x, ap_uint<2> sel) {
#pragma HLS INLINE
    return gamma_lut_g[(unsigned)sel][(unsigned)x];
}

static inline ap_uint<8> gamma_lookup_b(ap_uint<8> x, ap_uint<2> sel) {
#pragma HLS INLINE
    return gamma_lut_b[(unsigned)sel][(unsigned)x];
}

static inline ap_uint<24> wb_stage(
    ap_uint<24> in_rgb,
    ap_uint<12> wb_r_gain,
    ap_uint<12> wb_g_gain,
    ap_uint<12> wb_b_gain
) {
#pragma HLS INLINE
    ap_uint<8> r, g, b;
    unpack_rgb(in_rgb, r, g, b);

    ap_uint<8> r_wb = wb_gain_u8(r, wb_r_gain);
    ap_uint<8> g_wb = wb_gain_u8(g, wb_g_gain);
    ap_uint<8> b_wb = wb_gain_u8(b, wb_b_gain);

    return pack_rgb(r_wb, g_wb, b_wb);
}

static inline ap_uint<24> gamma_stage(ap_uint<24> in_rgb, ap_uint<2> gamma_sel) {
#pragma HLS INLINE
    ap_uint<8> r, g, b;
    unpack_rgb(in_rgb, r, g, b);

    ap_uint<8> r_gm = gamma_lookup_r(r, gamma_sel);
    ap_uint<8> g_gm = gamma_lookup_g(g, gamma_sel);
    ap_uint<8> b_gm = gamma_lookup_b(b, gamma_sel);

    return pack_rgb(r_gm, g_gm, b_gm);
}

static inline ap_uint<24> denoise_stage(
    ap_uint<24> cur_gamma,
    bool left_valid,
    ap_uint<24> left_gamma,
    bool up_valid,
    ap_uint<24> up_gamma
) {
#pragma HLS INLINE
    ap_uint<8> cr, cg, cb;
    ap_uint<8> lr, lg, lb;
    ap_uint<8> ur, ug, ub;

    unpack_rgb(cur_gamma,  cr, cg, cb);
    unpack_rgb(left_gamma, lr, lg, lb);
    unpack_rgb(up_gamma,   ur, ug, ub);

    if (!left_valid) {
        lr = cr; lg = cg; lb = cb;
    }
    if (!up_valid) {
        ur = cr; ug = cg; ub = cb;
    }

    // 最大值为 4*255 + 2*255 + 2*255 = 2040，需要 11bit。
    // 原先若用 10bit，会在结果 >= 1024 时回绕，导致输出少 128。
    ap_uint<11> r_sum = ((ap_uint<11>)cr << 2) + ((ap_uint<11>)lr << 1) + ((ap_uint<11>)ur << 1);
    ap_uint<11> g_sum = ((ap_uint<11>)cg << 2) + ((ap_uint<11>)lg << 1) + ((ap_uint<11>)ug << 1);
    ap_uint<11> b_sum = ((ap_uint<11>)cb << 2) + ((ap_uint<11>)lb << 1) + ((ap_uint<11>)ub << 1);

    ap_uint<8> r_dn = (ap_uint<8>)(r_sum >> 3);
    ap_uint<8> g_dn = (ap_uint<8>)(g_sum >> 3);
    ap_uint<8> b_dn = (ap_uint<8>)(b_sum >> 3);

    return pack_rgb(r_dn, g_dn, b_dn);
}

void video_preprocess(
    hls::stream<axis_rgb_t>& src,
    hls::stream<axis_rgb_t>& dst,
    int rows,
    int cols,
    int roi_x,
    int roi_y,
    int roi_w,
    int roi_h,
    ap_uint<12> wb_r_gain,
    ap_uint<12> wb_g_gain,
    ap_uint<12> wb_b_gain,
    ap_uint<2>  gamma_sel
) {
#pragma HLS INTERFACE axis port=src
#pragma HLS INTERFACE axis port=dst

#pragma HLS INTERFACE s_axilite port=rows       bundle=CTRL
#pragma HLS INTERFACE s_axilite port=cols       bundle=CTRL
#pragma HLS INTERFACE s_axilite port=roi_x      bundle=CTRL
#pragma HLS INTERFACE s_axilite port=roi_y      bundle=CTRL
#pragma HLS INTERFACE s_axilite port=roi_w      bundle=CTRL
#pragma HLS INTERFACE s_axilite port=roi_h      bundle=CTRL
#pragma HLS INTERFACE s_axilite port=wb_r_gain  bundle=CTRL
#pragma HLS INTERFACE s_axilite port=wb_g_gain  bundle=CTRL
#pragma HLS INTERFACE s_axilite port=wb_b_gain  bundle=CTRL
#pragma HLS INTERFACE s_axilite port=gamma_sel  bundle=CTRL
#pragma HLS INTERFACE s_axilite port=return     bundle=CTRL

    static ap_uint<24> prev_row_gamma[MAX_COLS];
    static ap_uint<1>  prev_row_valid[MAX_COLS];
#pragma HLS BIND_STORAGE variable=prev_row_gamma type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=prev_row_valid type=ram_2p impl=lutram
#pragma HLS DEPENDENCE variable=prev_row_gamma inter false
#pragma HLS DEPENDENCE variable=prev_row_valid inter false

ROW_LOOP:
    for (int y = 0; y < rows; y++) {
        ap_uint<24> left_gamma = 0;
        bool left_valid = false;

    COL_LOOP:
        for (int x = 0; x < cols; x++) {
#pragma HLS PIPELINE II=1
            axis_rgb_t in_pix = src.read();
            axis_rgb_t out_pix = in_pix;

            const bool in_roi =
                (x >= roi_x) &&
                (x <  roi_x + roi_w) &&
                (y >= roi_y) &&
                (y <  roi_y + roi_h);

            ap_uint<24> out_data = 0;

            if (in_roi) {
                ap_uint<24> wb_out    = wb_stage((ap_uint<24>)in_pix.data, wb_r_gain, wb_g_gain, wb_b_gain);
                ap_uint<24> gamma_out = gamma_stage(wb_out, gamma_sel);

                ap_uint<24> up_gamma  = prev_row_gamma[x];
                bool up_valid = ((y > roi_y) && (prev_row_valid[x] == 1));

                out_data = denoise_stage(gamma_out, left_valid, left_gamma, up_valid, up_gamma);

                left_gamma        = gamma_out;
                left_valid        = true;
                prev_row_gamma[x] = gamma_out;
                prev_row_valid[x] = 1;
            } else {
                out_data = 0;

                left_gamma        = 0;
                left_valid        = false;
                prev_row_gamma[x] = 0;
                prev_row_valid[x] = 0;
            }

            out_pix.data = out_data;
            dst.write(out_pix);
        }
    }
}
