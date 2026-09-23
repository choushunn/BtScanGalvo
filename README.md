# OCT_CT

STM32F407 振镜（Galvo）扫描控制固件，用于 OCT/CT 扫描光学系统的振镜驱动与控制。

## 项目简介

本项目基于 **STM32F407VETx**（HAL 库 + FreeRTOS），通过 **SPI** 控制 **DAC8563** 双通道 DAC，驱动双轴振镜（X 轴 → DAC-A，Y 轴 → DAC-B），以查找表方式在振镜控制平面上绘制预设形状，实现激光扫描。

## 硬件与外设

- 主控：STM32F407VETx
- 系统：FreeRTOS（CMSIS-OS v1）
- 外设使用：SPI（DAC8563）、UART/USART、GPIO、TIM（系统节拍）
- 显示：DAC8563 内部基准使能，A/B 增益 ×2 → 输出 0~5V 满幅

## 功能说明

- 通过 255 点正弦查找表生成 X/Y 波形，实时计算振镜形状点（最多 512 点）
- 按键切换绘制形状：KEY1(PD3) 正方形、KEY2(PD4) 长方形（宽:高=2:1）、KEY3(PD5) 圆形
- 相邻点间隔 30us（256 点约 110Hz 重绘），人眼可观察到完整形状
- 幅度可调（满量程百分比，默认 100%）

## 目录结构

```
OCT_CT/
├── Core/          用户代码（含 FreeRTOS 应用、main、外设驱动）
├── Drivers/       ST 官方 HAL 驱动
├── Middlewares/   FreeRTOS 中间件
├── MDK-ARM/       Keil MDK 工程（.uvprojx）
└── OCT_CT.ioc     STM32CubeMX 工程文件
```

## 构建

使用 Keil MDK-ARM 打开 `MDK-ARM/OCT_CT.uvprojx`，编译后通过 ST-Link 烧录。硬件配置由 `OCT_CT.ioc` 生成（STM32CubeMX 可重新生成工程）。

## License

Copyright (c) 2026 STMicroelectronics. All rights reserved.