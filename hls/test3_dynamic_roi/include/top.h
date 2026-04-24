#ifndef TOP_H
#define TOP_H

#include <ap_int.h>
#include <ap_axi_sdata.h>
#include <hls_stream.h>

#define MAX_COLS 1920
#define MAX_ROWS 1080

typedef ap_axiu<24, 1, 1, 1> axis_rgb_t;

// video_preprocess
// AXI4-Stream RGB in  -> ROI -> WB -> Gamma -> Denoise -> AXI4-Stream RGB out
// ROI 外输出清零
void video_preprocess(
    hls::stream<axis_rgb_t>& src,
    hls::stream<axis_rgb_t>& dst,
    int rows,
    int cols,
    int roi_x,
    int roi_y,
    int roi_w,
    int roi_h,
    ap_uint<12> wb_r_gain,   // Q4.8, 256 = 1.0
    ap_uint<12> wb_g_gain,   // Q4.8, 256 = 1.0
    ap_uint<12> wb_b_gain,   // Q4.8, 256 = 1.0
    ap_uint<2>  gamma_sel    // 0/1/2/3 -> preset gamma LUT
);

#endif // TOP_H
