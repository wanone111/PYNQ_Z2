# Video Preprocess HLS IP - 中文说明

## 概述

这是一个面向 PYNQ-Z2 的 HLS 视频预处理 IP，当前主功能是：

1. 轻量 ISP 处理：白平衡、Gamma 校正、去噪
2. 动态 ROI：根据外部参数对指定区域内像素进行处理
3. AXI4-Stream 输出：为后续视频链路提供标准流接口

RGB/BGR 到 YUV420SP（NV12）的颜色空间转换细节已拆分到独立文档：
[RGB2YUV 说明](README_RGB2YUV.md)

## 处理流程

```text
输入 RGB/BGR 视频流
    ↓
白平衡
    ↓
Gamma 校正
    ↓
去噪
    ↓
ROI 门控（ROI 外像素清零）
    ↓
输出 AXI4-Stream
```

## 特性

- AXI4-Stream 接口，适合接入 Xilinx 视频管线
- S_AXILITE 控制口，运行时可配置分辨率、ROI 和 ISP 参数
- DATAFLOW 和流水线优化，提升吞吐率
- 线缓冲与 BRAM 存储，适合 HLS 综合

## 参数

| 参数 | 类型 | 说明 | 取值范围 |
| --- | --- | --- | --- |
| `rows` | ap_uint<16> | 帧高 | 1-720 |
| `cols` | ap_uint<16> | 帧宽 | 1-1280 |
| `roi_x` | ap_uint<16> | ROI 左上角 X 坐标 | 0-cols-1 |
| `roi_y` | ap_uint<16> | ROI 左上角 Y 坐标 | 0-rows-1 |
| `roi_w` | ap_uint<16> | ROI 宽度 | 1-cols |
| `roi_h` | ap_uint<16> | ROI 高度 | 1-rows |
| `wb_r_gain` | ap_uint<16> | 白平衡 R 增益（Q8） | 0-65535，256 表示 1.0x |
| `wb_g_gain` | ap_uint<16> | 白平衡 G 增益（Q8） | 0-65535 |
| `wb_b_gain` | ap_uint<16> | 白平衡 B 增益（Q8） | 0-65535 |
| `gamma_mode` | ap_uint<8> | Gamma 模式 | 0=BYPASS，1=BRIGHT，2=DARK，3=DARK_STRONG |
| `denoise_strength` | ap_uint<8> | 去噪强度 | 0-255 |
| `input_is_bgr` | ap_uint<8> | 输入格式 | 0=RGB，1=BGR |

## 资源与性能

以下为设计目标级别的估算值，实际结果以 HLS 综合报告为准：

- LUT：约 8,000 - 12,000
- FF：约 6,000 - 9,000
- BRAM：约 10 - 15
- DSP48：约 6 - 9
- 延迟：约 2 - 3 行
- 吞吐率：1 像素 / 时钟周期（II=1）

## 构建说明

### 环境要求

- Vivado HLS 2020.2 或更高版本
- Vitis HLS 2021.1 或更高版本

### 运行综合

```bash
cd /workspace/hls/video_preprocess
vitis-run --mode hls --tcl scripts/run_hls.tcl
```

### 运行 C 仿真

```bash
vitis-run --mode hls --tcl scripts/run_hls.tcl
```

### 导出 IP

```bash
vitis-run --mode hls --tcl scripts/run_hls.tcl
```

## 集成说明

综合完成后，可以将该 IP 接入 Vivado Block Design，与 Zynq PS、AXI DMA 和后续视频处理模块组合使用。

## 许可证

MIT License
