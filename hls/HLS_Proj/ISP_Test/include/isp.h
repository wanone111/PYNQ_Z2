#ifndef PYNQ_ISP_H_
#define PYNQ_ISP_H_

#include <ap_int.h>

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
                       ap_uint<8>& out_b);

#endif  // PYNQ_ISP_H_
