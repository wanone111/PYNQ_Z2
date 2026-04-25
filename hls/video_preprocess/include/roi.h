#ifndef PYNQ_ROI_H_
#define PYNQ_ROI_H_

#include <ap_int.h>
#include "video_params.h"

// ============================================================================
// ROI Helper Functions - Inline implementations for zero-overhead checks
// ============================================================================

// Pack ROI parameters into window boundaries with clamping to frame bounds
static inline RoiWindow pack_roi_window(ap_uint<16> rows,
                                         ap_uint<16> cols,
                                         ap_uint<16> roi_x,
                                         ap_uint<16> roi_y,
                                         ap_uint<16> roi_w,
                                         ap_uint<16> roi_h) {
#pragma HLS INLINE
    RoiWindow roi;

    // Clamp dimensions to valid range
    ap_uint<16> safe_rows = (rows > MAX_HEIGHT) ? MAX_HEIGHT : ((rows == 0) ? 1 : rows);
    ap_uint<16> safe_cols = (cols > MAX_WIDTH) ? MAX_WIDTH : ((cols == 0) ? 1 : cols);

    // Clamp ROI origin to frame bounds
    roi.x0 = (roi_x >= safe_cols) ? (safe_cols - 1) : roi_x;
    roi.y0 = (roi_y >= safe_rows) ? (safe_rows - 1) : roi_y;

    // Handle zero width/height by using full frame
    ap_uint<16> w = (roi_w == 0) ? safe_cols : roi_w;
    ap_uint<16> h = (roi_h == 0) ? safe_rows : roi_h;

    // Calculate end coordinates
    roi.x1 = roi.x0 + w;
    roi.y1 = roi.y0 + h;

    // Clamp end coordinates to frame bounds
    if (roi.x1 > safe_cols) roi.x1 = safe_cols;
    if (roi.y1 > safe_rows) roi.y1 = safe_rows;

    return roi;
}

// Check if pixel (row, col) is inside ROI using closed-open interval
static inline bool roi_contains(const RoiWindow& roi, ap_uint<16> row, ap_uint<16> col) {
#pragma HLS INLINE
    return (row >= roi.y0) && (row < roi.y1) && (col >= roi.x0) && (col < roi.x1);
}

#endif  // PYNQ_ROI_H_
