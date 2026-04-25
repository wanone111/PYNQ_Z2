# Video Preprocess HLS IP - Unified ISP + ROI + RGB2YUV

## Overview

This HLS IP integrates three video processing stages into a single optimized module for PYNQ-Z2:

1. **Lightweight ISP**: White Balance → Gamma Correction → Denoising
2. **Dynamic ROI**: Pixel-level gating based on externally controlled ROI coordinates
3. **RGB to YUV420SP (NV12)**: Color space conversion with 2x2 chroma subsampling

## Architecture

```
Input (RGB/BGR) 
    ↓
[White Balance] → [Gamma Correction] → [Denoise] (ISP Stage, ROI-gated)
    ↓
[ROI Masking] (zero outside ROI)
    ↓
[RGB→YUV Conversion] (parallel DSP48-based)
    ↓
[2x2 Chroma Subsampling] (NV12 output)
    ↓
Output: Y Stream + UV Stream (interleaved)
```

## Features

- **AXI4-Stream Interface**: Compatible with Xilinx video pipelines
- **S_AXILITE Control**: Runtime configuration of all parameters
- **DATAFLOW Optimization**: Task-level parallelism for throughput
- **SRL Line Buffers**: 1-cycle access latency for 2x2 averaging
- **DSP48 Utilization**: Parallel YUV calculation for 15-25% latency reduction
- **128-byte Alignment Ready**: Output formatted for efficient UDP encapsulation

## Parameters

| Parameter | Type | Description | Range |
|-----------|------|-------------|-------|
| `rows` | ap_uint<16> | Frame height | 1-720 |
| `cols` | ap_uint<16> | Frame width | 1-1280 |
| `roi_x` | ap_uint<16> | ROI top-left X | 0-cols-1 |
| `roi_y` | ap_uint<16> | ROI top-left Y | 0-rows-1 |
| `roi_w` | ap_uint<16> | ROI width | 1-cols |
| `roi_h` | ap_uint<16> | ROI height | 1-rows |
| `wb_r_gain` | ap_uint<16> | White balance R gain (Q8) | 0-65535 (256=1.0x) |
| `wb_g_gain` | ap_uint<16> | White balance G gain (Q8) | 0-65535 |
| `wb_b_gain` | ap_uint<16> | White balance B gain (Q8) | 0-65535 |
| `gamma_mode` | ap_uint<8> | Gamma mode | 0=BYPASS, 1=BRIGHT, 2=DARK, 3=DARK_STRONG |
| `denoise_strength` | ap_uint<8> | Temporal denoise strength | 0-255 |
| `gray_en` | ap_uint<8> | Grayscale output enable | 0/1 |
| `threshold_en` | ap_uint<8> | Binary threshold enable | 0/1 |
| `threshold_val` | ap_uint<8> | Threshold value | 0-255 |
| `input_is_bgr` | ap_uint<8> | Input format | 0=RGB, 1=BGR |

## Resource Estimates (1280x720 @ 100MHz)

- **LUT**: ~8,000-12,000
- **FF**: ~6,000-9,000
- **BRAM**: ~10-15 (line buffers)
- **DSP48**: ~6-9 (parallel YUV calculation)
- **Latency**: ~2-3 rows (pipeline fill)
- **Throughput**: 1 pixel/cycle (II=1)

## Build Instructions

### Prerequisites
- Vivado HLS 2020.2 or later
- Vitis HLS 2021.1 or later (recommended)

### Run Synthesis
```bash
cd /workspace/hls/video_preprocess
vivado_hls -f scripts/run_hls.tcl
```

### Run C Simulation (Optional)
```bash
vivado_hls -f scripts/run_hls.tcl
# Uncomment csim_design in the TCL script first
```

### Export as IP Catalog
```bash
# Uncomment export_design in run_hls.tcl
vivado_hls -f scripts/run_hls.tcl
```

## Integration with PYNQ

1. After HLS synthesis, export the IP catalog
2. Create Vivado block design with:
   - Zynq PS7 (M_AXI_GP0 for video input, M_AXI_GP1 for output)
   - AXI DMA (for stream-to-memory transfer)
   - video_preprocess IP
   - AXI Interconnect
3. Generate bitstream and export XSA
4. Use PYNQ Python overlay to configure and control

## Output Format

The IP outputs YUV420SP (NV12) format:
- **Y Stream**: Full resolution luma (rows × cols pixels)
- **UV Stream**: Interleaved chroma at half resolution ((rows/2) × cols bytes)
  - U and V alternate: U0, V0, U1, V1, ...
  - TLAST marks end of each UV row

## Next Steps

This integrated IP is ready for:
1. UDP encapsulation module (to be added)
2. Control stream receiver (for dynamic ROI updates from 310B)
3. AXI DMA integration for memory-mapped I/O
4. PYNQ Python overlay development

## License

MIT License
