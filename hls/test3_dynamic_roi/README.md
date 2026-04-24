# test3_dynamic_roi

这是按你当前任务重新整理的一套 **完整 HLS 工程骨架**，对应的新 `video_preprocess` IP 逻辑为：

**ROI 内：WB -> Gamma -> Denoise**  
**ROI 外：输出 0**

不再保留旧版灰度 / 阈值主链，也不再叠加第二套轻量 ISP。

---

## 目录结构

```text
test3_dynamic_roi/
├── build/
├── include/
│   ├── gamma_lut.h
│   └── top.h
├── logs/
├── scripts/
│   └── run_hls.tcl
├── src/
│   └── top.cpp
└── tb/
    └── testbench.cpp
```

---

## 顶层接口

顶层函数名保持不变：

```cpp
void video_preprocess(
    hls::stream<axis_rgb_t>& src,
    hls::stream<axis_rgb_t>& dst,
    int rows,
    int cols,
    int roi_x,
    int roi_y,
    int roi_w,
    int roi_h,
    ap_uint<12> wb_r_gain,
    ap_uint<12> wb_g_gain,
    ap_uint<12> wb_b_gain,
    ap_uint<2>  gamma_sel
);
```

### 控制参数说明

- `rows, cols`：帧高、帧宽
- `roi_x, roi_y, roi_w, roi_h`：运行时 ROI
- `wb_r_gain, wb_g_gain, wb_b_gain`：Q4.8 白平衡增益
  - `256 = 1.0`
  - `320 = 1.25`
  - `224 = 0.875`
- `gamma_sel`：Gamma LUT 选择
  - `0`：identity
  - `1`：gamma=0.8
  - `2`：gamma=1.2
  - `3`：gamma=1.5

---

## 新版内部处理链

```text
AXI4-Stream RGB 输入
-> ROI 判断
-> ROI 内:
   White Balance
   -> Gamma LUT
   -> Simple Denoise(当前 + 左邻 + 上邻)
-> ROI 外输出 0
-> AXI4-Stream RGB 输出
```

### Denoise 形式

采用固定轻量核：

```text
out = (4*cur + 2*left + 2*up) >> 3
```

特点：

- 只需要 1 行 line buffer
- 可流式
- 资源轻
- 适合 PYNQ-Z2

---

## 运行方式

在工程根目录下执行：

```bash
vitis_hls -f scripts/run_hls.tcl
```

脚本会依次完成：

- csim
- csynth
- export ip_catalog

---

## 说明

这版是按你的新要求重新做的，不是旧版局部修补版。  
接口形式保持 AXI4-Stream + AXI-Lite，不涉及系统级链路其它模块。
