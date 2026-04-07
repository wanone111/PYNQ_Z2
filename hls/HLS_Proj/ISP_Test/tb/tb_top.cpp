#include <cstdint>
#include <iostream>

#include "top.h"

static axi_pixel_t make_pixel(uint8_t r, uint8_t g, uint8_t b, bool last) {
	axi_pixel_t p;
	p.data.range(7, 0) = b;
	p.data.range(15, 8) = g;
	p.data.range(23, 16) = r;
	p.keep = -1;
	p.strb = -1;
	p.user = 0;
	p.last = last ? 1 : 0;
	p.id = 0;
	p.dest = 0;
	return p;
}

int main() {
	const ap_uint<16> rows = 4;
	const ap_uint<16> cols = 4;
	const uint8_t in_r = 40;
	const uint8_t in_g = 40;
	const uint8_t in_b = 200;

	hls::stream<axi_pixel_t> src;
	hls::stream<axi_pixel_t> dst;

	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			src.write(make_pixel(in_r, in_g, in_b, (c == cols - 1)));
		}
	}

	video_preprocess(
		src,
		dst,
		rows,
		cols,
		0,
		128,
		0,
		0,
		0,
		2,
		2,
		2,
		2,
		0,
		0,
		96);

	int total = 0;
	int nonzero_outside_roi = 0;
	int passthrough_mismatch = 0;

	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			axi_pixel_t p = dst.read();
			++total;

			uint8_t b = (uint8_t)p.data.range(7, 0);
			uint8_t g = (uint8_t)p.data.range(15, 8);
			uint8_t rr = (uint8_t)p.data.range(23, 16);

			bool in_roi = (r >= 2 && r < 6 && c >= 2 && c < 6);
			bool same_as_input = (rr == in_r && g == in_g && b == in_b);
			bool nonzero = (rr != 0 || g != 0 || b != 0);

			if (in_roi && !same_as_input) {
				++passthrough_mismatch;
			}
			if (!in_roi && nonzero) {
				++nonzero_outside_roi;
			}
		}
	}

	std::cout << "[TB-CASE1] total=" << total
			  << " passthrough_mismatch=" << passthrough_mismatch
			  << " nonzero_outside_roi=" << nonzero_outside_roi << std::endl;

	if (total != rows * cols) {
		std::cerr << "[TB][FAIL] output pixel count mismatch" << std::endl;
		return 1;
	}
	if (passthrough_mismatch != 0) {
		std::cerr << "[TB][FAIL] ISP off passthrough mismatch" << std::endl;
		return 1;
	}
	if (nonzero_outside_roi != 0) {
		std::cerr << "[TB][FAIL] ROI masking failed" << std::endl;
		return 1;
	}

	hls::stream<axi_pixel_t> src2;
	hls::stream<axi_pixel_t> dst2;
	for (int r = 0; r < rows; ++r) {
		for (int c = 0; c < cols; ++c) {
			src2.write(make_pixel(in_r, in_g, in_b, (c == cols - 1)));
		}
	}

	video_preprocess(
		src2,
		dst2,
		rows,
		cols,
		1,
		128,
		20,
		1,
		128,
		0,
		0,
		0,
		0,
		0,
		0,
		96);

	axi_pixel_t p0 = dst2.read();
	uint8_t out_b = (uint8_t)p0.data.range(7, 0);
	uint8_t out_g = (uint8_t)p0.data.range(15, 8);
	uint8_t out_r = (uint8_t)p0.data.range(23, 16);

	for (int i = 1; i < rows * cols; ++i) {
		( void )dst2.read();
	}

	std::cout << "[TB-CASE2] in_rgb=(" << (int)in_r << "," << (int)in_g << "," << (int)in_b
			  << ") out_rgb=(" << (int)out_r << "," << (int)out_g << "," << (int)out_b << ")" << std::endl;

	if (!(out_r > in_r)) {
		std::cerr << "[TB][FAIL] brightness/AWB did not lift red channel" << std::endl;
		return 1;
	}
	if (!(out_b < (uint8_t)(in_b + 20))) {
		std::cerr << "[TB][FAIL] AWB did not reduce blue cast" << std::endl;
		return 1;
	}

	std::cout << "[TB][PASS] ROI + lightweight ISP behavior is correct" << std::endl;
	return 0;
}
