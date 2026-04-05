# PYNQ-Z2 侧实现说明

本目录用于承接赛题中的 PYNQ 侧工作，目标是完成前端视频流预处理并与 310B 协同。

## 1. 角色分工对齐

- PYNQ-Z2: 采集接口适配、轻量预处理、ROI 流控、稳定输出
- 310B: DVPP 几何整形、AIPP 数学预处理、NPU 推理、业务事件判定

原则: 只在一侧执行一次缩放和归一化，避免重复计算与带宽浪费。

## 2. 当前实现骨架

已落地的最小可综合骨架在 HLS 子目录：

- 头文件参数: [hls/include/video_params.h](hls/include/video_params.h)
- 顶层接口声明: [hls/include/top.h](hls/include/top.h)
- 顶层函数实现: [hls/src/top.cpp](hls/src/top.cpp)
- C 仿真测试: [hls/tb/tb_top.cpp](hls/tb/tb_top.cpp)
- HLS 脚本: [hls/scripts/run_hls.tcl](hls/scripts/run_hls.tcl)

当前 HLS 内核功能:

- AXI4-Stream 输入输出
- 运行时 ROI 裁剪窗口控制
- ROI 内灰度转换
- ROI 内二值化阈值处理
- ROI 外区域清零输出

该骨架用于先打通接口与时序，不绑定具体模型。

## 3. 顶层接口说明

函数名: video_preprocess

输入输出:

- src: AXI4-Stream 视频输入 (24-bit RGB)
- dst: AXI4-Stream 视频输出 (24-bit RGB)

控制参数 (AXI-Lite):

- rows, cols: 当前帧分辨率
- roi_x, roi_y, roi_w, roi_h: ROI 起点和尺寸
- gray_en: 灰度开关
- threshold_en: 二值化开关
- threshold_val: 二值化阈值

## 4. 团队开发建议

第一阶段 (已完成):

- 完成可综合骨架和 testbench 闭环

第二阶段 (进行中):

- 将 ROI 外清零改为可配置模式 (透传/清零)
- 补充基础滤波算子 (median 或 gaussian)

第三阶段 (待开始):

- 按赛题场景加入官方算子流水 (resize/cvtColor/gamma)
- 与 310B 侧接口协议联调

## 5. 运行方式

参考本机环境经验，优先使用:

```bash
vitis-run --mode hls --tcl hls/scripts/run_hls.tcl
```

如需 GUI 调试，可在 Vitis HLS 中直接打开脚本对应工程。

## 6. 参考

- [AMD Vitis Libraries](https://github.com/Xilinx/Vitis_Libraries)
