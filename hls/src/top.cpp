#include "top.h"

static inline ap_uint<16> clamp_u16(ap_uint<16> v, ap_uint<16> lo, ap_uint<16> hi) {
#pragma HLS INLINE
	if (v < lo) {
		return lo;
	}
	if (v > hi) {
		return hi;
	}
	return v;
}

void video_preprocess(
	hls::stream<axi_pixel_t>& src,
	hls::stream<axi_pixel_t>& dst,
	ap_uint<16> rows,
	ap_uint<16> cols,
	ap_uint<16> roi_x,
	ap_uint<16> roi_y,
	ap_uint<16> roi_w,
	ap_uint<16> roi_h,
	ap_uint<8> gray_en,
	ap_uint<8> threshold_en,
	ap_uint<8> threshold_val) {
#pragma HLS INTERFACE axis port=src
#pragma HLS INTERFACE axis port=dst
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=roi_x bundle=control
#pragma HLS INTERFACE s_axilite port=roi_y bundle=control
#pragma HLS INTERFACE s_axilite port=roi_w bundle=control
#pragma HLS INTERFACE s_axilite port=roi_h bundle=control
#pragma HLS INTERFACE s_axilite port=gray_en bundle=control
#pragma HLS INTERFACE s_axilite port=threshold_en bundle=control
#pragma HLS INTERFACE s_axilite port=threshold_val bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

	ap_uint<16> proc_rows = clamp_u16(rows, 1, MAX_HEIGHT);
	ap_uint<16> proc_cols = clamp_u16(cols, 1, MAX_WIDTH);

	ap_uint<16> x0 = clamp_u16(roi_x, 0, (proc_cols > 0) ? (ap_uint<16>)(proc_cols - 1) : (ap_uint<16>)0);
	ap_uint<16> y0 = clamp_u16(roi_y, 0, (proc_rows > 0) ? (ap_uint<16>)(proc_rows - 1) : (ap_uint<16>)0);
	ap_uint<16> rw = (roi_w == 0) ? (ap_uint<16>)proc_cols : roi_w;
	ap_uint<16> rh = (roi_h == 0) ? (ap_uint<16>)proc_rows : roi_h;

	ap_uint<16> x1 = x0 + rw;
	ap_uint<16> y1 = y0 + rh;
	if (x1 > proc_cols) {
		x1 = proc_cols;
	}
	if (y1 > proc_rows) {
		y1 = proc_rows;
	}

row_loop:
	for (ap_uint<16> r = 0; r < proc_rows; ++r) {
	col_loop:
		for (ap_uint<16> c = 0; c < proc_cols; ++c) {
#pragma HLS PIPELINE II=1
			axi_pixel_t in_pix = src.read();
			axi_pixel_t out_pix;

			ap_uint<8> b = in_pix.data.range(7, 0);
			ap_uint<8> g = in_pix.data.range(15, 8);
			ap_uint<8> rr = in_pix.data.range(23, 16);

			bool in_roi = (r >= y0) && (r < y1) && (c >= x0) && (c < x1);

			ap_uint<8> out_r = 0;
			ap_uint<8> out_g = 0;
			ap_uint<8> out_b = 0;

			if (in_roi) {
				if (gray_en) {
					ap_uint<16> y = (ap_uint<16>)rr * 77 + (ap_uint<16>)g * 150 + (ap_uint<16>)b * 29;
					ap_uint<8> gray = (ap_uint<8>)(y >> 8);
					if (threshold_en) {
						ap_uint<8> bin = (gray > threshold_val) ? (ap_uint<8>)255 : (ap_uint<8>)0;
						out_r = bin;
						out_g = bin;
						out_b = bin;
					} else {
						out_r = gray;
						out_g = gray;
						out_b = gray;
					}
				} else {
					out_r = rr;
					out_g = g;
					out_b = b;
				}
			}

			out_pix.data.range(7, 0) = out_b;
			out_pix.data.range(15, 8) = out_g;
			out_pix.data.range(23, 16) = out_r;
			out_pix.keep = in_pix.keep;
			out_pix.strb = in_pix.strb;
			out_pix.user = (r == 0 && c == 0) ? 1 : 0;
			out_pix.last = (c == (proc_cols - 1)) ? 1 : 0;
			out_pix.id = in_pix.id;
			out_pix.dest = in_pix.dest;

			dst.write(out_pix);
		}
	}
}
