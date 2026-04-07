# PYNQ-Z2 侧实现说明

本目录用于承接赛题中的 PYNQ 侧工作，目标是完成前端视频流预处理并与 310B 协同。

## 1. 当前目录结构（已对齐仓库现状）

- HLS 工程主路径: [hls/HLS_Proj/ROI_Test](hls/HLS_Proj/ROI_Test)
- PS 侧脚本路径: [ps](ps)
- PL 侧比特流说明: [pl/README.md](pl/README.md)

当前 HLS 可综合代码入口：

- 顶层声明: [hls/HLS_Proj/ROI_Test/include/top.h](hls/HLS_Proj/ROI_Test/include/top.h)
- 视频参数: [hls/HLS_Proj/ROI_Test/include/video_params.h](hls/HLS_Proj/ROI_Test/include/video_params.h)
- 顶层实现: [hls/HLS_Proj/ROI_Test/src/top.cpp](hls/HLS_Proj/ROI_Test/src/top.cpp)
- Testbench: [hls/HLS_Proj/ROI_Test/tb/tb_top.cpp](hls/HLS_Proj/ROI_Test/tb/tb_top.cpp)
- HLS 脚本: [hls/HLS_Proj/ROI_Test/scripts/run_hls.tcl](hls/HLS_Proj/ROI_Test/scripts/run_hls.tcl)

## 2. PYNQ 与 310B 侧分工

- PYNQ-Z2: 采集接口适配、轻量预处理、ROI 流控、稳定输出
- 310B: DVPP 几何整形、AIPP 数学预处理、NPU 推理、业务事件判定

原则：只在一侧执行一次缩放和归一化，避免重复计算与带宽浪费。

## 3. 当前 HLS 内核能力

当前顶层函数为 video_preprocess，接口在 [hls/HLS_Proj/ROI_Test/include/top.h](hls/HLS_Proj/ROI_Test/include/top.h)。

输入输出：

- src: AXI4-Stream 视频输入（24-bit RGB）
- dst: AXI4-Stream 视频输出（24-bit RGB）

控制参数（AXI-Lite）：

- rows, cols: 当前帧分辨率
- roi_x, roi_y, roi_w, roi_h: ROI 起点和尺寸
- gray_en: 灰度开关
- threshold_en: 二值化开关
- threshold_val: 二值化阈值

当前实现行为：

- 支持运行时 ROI 窗口控制
- ROI 内支持灰度转换
- ROI 内可选二值化阈值处理
- ROI 外区域输出清零

注：编译期最大分辨率约束定义在 [hls/HLS_Proj/ROI_Test/include/video_params.h](hls/HLS_Proj/ROI_Test/include/video_params.h)（MAX_WIDTH=1280, MAX_HEIGHT=720）。

## 4. PS 侧桥接脚本

USB 摄像头到 PL 的桥接脚本：

- 主脚本: [ps/usb_camera_to_pl.py](ps/usb_camera_to_pl.py)
- Jupyter 版本: [ps/usb_camera_to_pl.ipynb](ps/usb_camera_to_pl.ipynb)
- 摄像头预检脚本: [ps/usb_camera.py](ps/usb_camera.py)

[ps/usb_camera_to_pl.py](ps/usb_camera_to_pl.py) 当前支持：

- 从 USB 摄像头读取 BGR 帧
- 将图像打包为 32-bit RGB 数据流（便于 DMA/VDMA 对接）
- 向 video_preprocess IP 写控制寄存器
- dry-run 模式（无板卡环境先验证采集和打包流程）
- 每秒统计 fps、桥接耗时、发送失败与丢帧估计

## 5. 运行方式

在本机环境下，建议通过 vitis-run 启动 HLS：

```bash
cd /home/bb/competition_ws/pynq_z2
vitis-run --mode hls --tcl hls/HLS_Proj/ROI_Test/scripts/run_hls.tcl
```

PS 侧 dry-run 示例：

```bash
cd /home/bb/competition_ws/pynq_z2
python3 ps/usb_camera_to_pl.py --dry-run --camera 0 --width 640 --height 480 --show
```

摄像头预检示例：

```bash
cd /home/bb/competition_ws/pynq_z2
python3 ps/usb_camera_to_pl.py --probe --camera 0
```

上板运行示例（按实际 bit/hwh 与 IP 名称调整）：

```bash
cd /home/bb/competition_ws/pynq_z2
python3 ps/usb_camera_to_pl.py --overlay <overlay.bit> --ip-name video_preprocess --camera 0 --width 640 --height 480 --roi 0.1,0.1,0.8,0.8
```

## 6. 联调建议

- 先用 HLS testbench 验证 ROI/灰度/阈值行为，再上板联调。
- 先跑 dry-run，再切换 overlay + DMA 实链路，便于快速定位问题层级。
- PL 侧 bit/hwh 文件配对规则参考 [pl/README.md](pl/README.md)。

## 7. 参考

- AMD Vitis Libraries: [https://github.com/Xilinx/Vitis_Libraries](https://github.com/Xilinx/Vitis_Libraries)
