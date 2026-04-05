# PYNQ-Z2 侧实现说明

本目录用于承接赛题中的 PYNQ 侧工作，目标是完成前端视频流预处理并与 310B 协同。

## 1. 角色分工对齐

- PYNQ-Z2: 采集接口适配、轻量预处理、ROI 流控、稳定输出
- 310B: DVPP 几何整形、AIPP 数学预处理、NPU 推理、业务事件判定

原则: 只在一侧执行一次缩放和归一化，避免重复计算与带宽浪费。

## 2. 当前实现骨架

已落地的最小可综合骨架在 HLS 子目录：

- 头文件参数: [hls/include/video_params.h](hls/include/video_params.h)
- ROI 模块头文件: [hls/include/roi.h](hls/include/roi.h)
- 顶层接口声明: [hls/include/top.h](hls/include/top.h)
- 顶层函数实现: [hls/src/top.cpp](hls/src/top.cpp)
- ROI 模块实现: [hls/src/roi.cpp](hls/src/roi.cpp)
- C 仿真测试: [hls/tb/tb_top.cpp](hls/tb/tb_top.cpp)
- HLS 脚本: [hls/scripts/run_hls.tcl](hls/scripts/run_hls.tcl)

当前 HLS 内核功能:

- AXI4-Stream 输入输出
- 运行时 ROI 裁剪窗口控制
- ROI 内灰度转换
- ROI 内二值化阈值处理
- ROI 外区域清零输出

ROI 已经从顶层逻辑中拆出，成为独立模块，便于后续继续替换成更复杂的裁剪、缩放或 letterbox 策略。

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

## 5. USB 摄像头到 PL 数据通路

推荐的数据路径是:

USB 摄像头 -> PYNQ 侧 Linux/OpenCV 采集 -> AXI4-Stream 打包 -> PL HLS 内核 video_preprocess -> 输出给后续 PL/PS 链路

实现上，摄像头采集与 VDMA/AXI-Stream 传输不放在 HLS 内核里处理，而是由 PS 侧驱动把每帧图像喂给 PL。PL 侧只做像素流水处理，避免把设备控制逻辑和图像流水混在一起。

这意味着当前 HLS 内核的职责是“接收一帧流、按 ROI 规则处理、再送出一帧流”，USB 摄像头采集本身属于 PS 侧适配层。

PS 侧桥接脚本已经补齐骨架：

- [ps/usb_camera_to_pl.py](ps/usb_camera_to_pl.py)

该脚本完成三件事：

- 从 USB 摄像头读取 BGR 帧
- 将图像打包成 32-bit 像素流，便于与 PL 链路对接
- 在带 overlay 的情况下把控制参数写入 HLS IP，并通过 DMA/VDMA 完成发送与结果回读

当前桥接脚本还提供每秒统计：

- 采集帧率
- 发送帧率
- 回读帧率
- 读帧失败数
- 发送失败数
- 丢帧估计值

当前脚本支持 dry-run，可在普通 Linux 主机上先验证采集和打包逻辑；上板后再把 overlay 和 DMA 名称接到真实工程。

## 6. 预期效果

- ROI 拆分后，主核更清晰，后续替换裁剪或缩放策略时不需要改动顶层接口。
- USB 摄像头到 PL 的链路保持流式处理，减少 CPU 侧中间缓存与重复拷贝。
- 以当前实现规模看，PL 侧资源开销很低，适合先做稳定联调和功能闭环。
- 后续接入官方算子后，仍可保持单帧单流、单次处理的低延迟特征。
- USB 摄像头采集与 PL 运算分层后，团队可以独立调试采集链和 FPGA 链，减少联调耦合。
- DMA 回读链补齐后，可以直接验证 PL 侧输出是否与 ROI 和灰度/阈值控制一致。

## 7. 运行方式

参考本机环境经验，优先使用:

```bash
vitis-run --mode hls --tcl hls/scripts/run_hls.tcl
```

PS 侧桥接脚本示例:

```bash
python3 ps/usb_camera_to_pl.py --dry-run --camera 0 --width 640 --height 480 --show
```

Jupyter 版本入口:

- [ps/usb_camera_to_pl.ipynb](ps/usb_camera_to_pl.ipynb)

摄像头预检模式:

```bash
python3 ps/usb_camera_to_pl.py --probe --camera 0
```

上板后示例:

```bash
python3 ps/usb_camera_to_pl.py --overlay <overlay.bit> --camera 0 --width 640 --height 480 --roi 0.1,0.1,0.8,0.8
```

如需 GUI 调试，可在 Vitis HLS 中直接打开脚本对应工程。

## 8. 参考

- [AMD Vitis Libraries](https://github.com/Xilinx/Vitis_Libraries)
