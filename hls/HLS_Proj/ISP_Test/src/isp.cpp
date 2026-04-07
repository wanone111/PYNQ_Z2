#include "isp.h"

static inline ap_uint<8> sat_u8(ap_int<16> v) {
#pragma HLS INLINE
    if (v < 0) {
        return 0;
    }
    if (v > 255) {
        return 255;
    }
    return (ap_uint<8>)v;
}

void isp_process_pixel(ap_uint<8> in_r,
                       ap_uint<8> in_g,
                       ap_uint<8> in_b,
                       ap_uint<8> isp_en,
                       ap_uint<8> exposure_gain_q7,
                       ap_int<9> brightness_offset,
                       ap_uint<8> awb_en,
                       ap_uint<8> awb_strength,
                       ap_uint<8>& out_r,
                       ap_uint<8>& out_g,
                       ap_uint<8>& out_b) {
#pragma HLS INLINE
    if (!isp_en) {
        out_r = in_r;
        out_g = in_g;
        out_b = in_b;
        return;
    }

    ap_uint<8> gain = (exposure_gain_q7 == 0) ? (ap_uint<8>)128 : exposure_gain_q7;

    ap_int<16> r = ((ap_int<16>)in_r * (ap_int<16>)gain) >> 7;
    ap_int<16> g = ((ap_int<16>)in_g * (ap_int<16>)gain) >> 7;
    ap_int<16> b = ((ap_int<16>)in_b * (ap_int<16>)gain) >> 7;

    r += brightness_offset;
    g += brightness_offset;
    b += brightness_offset;

    r = sat_u8(r);
    g = sat_u8(g);
    b = sat_u8(b);

    if (awb_en) {
        ap_int<16> avg = (r + g + b) / 3;
        ap_int<16> dr = avg - r;
        ap_int<16> dg = avg - g;
        ap_int<16> db = avg - b;

        r += (dr * (ap_int<16>)awb_strength) >> 8;
        g += (dg * (ap_int<16>)awb_strength) >> 8;
        b += (db * (ap_int<16>)awb_strength) >> 8;
    }

    out_r = sat_u8(r);
    out_g = sat_u8(g);
    out_b = sat_u8(b);
}
