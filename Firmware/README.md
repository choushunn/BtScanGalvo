# BTScanGalvo

STM32F407 振镜（Galvo）扫描控制固件，用于 OCT/CT 扫描光学系统的振镜驱动与控制。

## 项目简介

基于 **STM32F407VETx**（HAL 库 + FreeRTOS），通过 **SPI** 控制 **DAC8563** 双通道 DAC 驱动双轴振镜（X 轴 → DAC-A，Y 轴 → DAC-B），以查找表方式在振镜控制平面上绘制预设形状，实现激光扫描。

## 硬件与外设

- 主控：STM32F407VETx
- 系统：FreeRTOS（CMSIS-OS v2）
- 外设：SPI3+DMA（DAC8563，SPI3_TX = **DMA1_Stream5/CH0**）、USART1（调试打印）、USART6（蓝牙 HC-05）、GPIO、TIM6（扫描点步进）、TIM7（系统节拍）
- 显示：DAC8563 内部基准使能，A/B 增益 ×2 → 输出 0~5V 满幅
- TIM6 与 SPI3_TX DMA 初始化由 **STM32CubeMX 生成**

## 架构

扫描点输出走硬件热路径，CPU 不参与逐点时序；低速、阻塞、多变的控制逻辑交给 RTOS 任务，两者通过命令队列解耦。

- 硬件热路径：TIM6 定住点间隔，更新中断经 `Scan_TimerTick()` 触发 SPI3_TX DMA 发出 X/Y 点帧（24bit），完成后翻转 SYNC 锁存并推进到下一个点，数据/时序全部脱 CPU。
- 应用层逻辑集中在自定义文件 **app_tasks.c**（BTTask + ScanCtrlTask + 命令队列），freertos.c 仅在 USER CODE 钩子调用 `App_Tasks_Init()`。
- BTTask：USART6 环形缓冲收字节 → 组帧 → 解析 → 投递命令并回 ACK。
- ScanCtrlTask：消费命令队列，在"安全点"停硬件 → 重建点表 → 重启硬件；同时约 10ms 轮询面板三键（PD3 循环切形状、PD4 启停、PD5 幅度 +10% 循环）。
- 状态隔离：点表与帧缓冲为单缓冲，仅停硬件后由 ScanCtrlTask 写入，ISR 只读，避免撕裂。

## 蓝牙命令协议（HC-05 透传）

帧格式：`[SOF 0xAA][LEN][CMD][Payload][CRC8]`，CRC8 覆盖 CMD+Payload。

| CMD | 含义 | Payload |
|-----|------|---------|
| 0x01 | 切换形状 | `shape`（0 正方/1 长方/2 圆） |
| 0x02 | 调扫描幅度 | `0~100`（满量程百分比） |
| 0x03 | 扫描启停 | `0` 停止 / `1` 启动 / `2` 暂停 |
| 0x04 | 调点间隔 | `interval_us`（小端 uint16，≥5us） |
| 0xF0 | ACK 回执 | `err`（0 正常 / 1 未知命令） |

命令字 `0x80` 及以上保留扩展。

## 功能说明

- 通过 255 点正弦查找表生成 X/Y 波形，实时计算振镜形状点（最多 4096 点）
- 面板三键（下降沿、10ms 轮询消抖）：PD3 循环切形状、PD4 启停切换、PD5 幅度 +10% 循环（10%~100%）
- 蓝牙（HC-05, USART6, 115200）可切形状、调幅度、启停/暂停、调点间隔
- 相邻点间隔默认 30us，幅度默认 100%，均可在线修改

## 目录结构

```
BtScanGalvo/
├── Core/          用户代码（app_tasks.c、scan.c、bt_*.c、main、外设驱动）
├── Drivers/       ST 官方 HAL 驱动
├── Middlewares/   FreeRTOS 中间件
├── MDK-ARM/       Keil MDK 工程（BtScanGalvo.uvprojx）
└── BtScanGalvo.ioc  STM32CubeMX 工程文件
```

## 构建

用 Keil 打开 `MDK-ARM/BtScanGalvo.uvprojx` 编译，经 ST-Link 烧录。硬件配置由 `BtScanGalvo.ioc` 经 STM32CubeMX 生成。

> **CubeMX 重新生成的注意事项**：驱动与 Middlewares 目录由 CubeMX 依据 .ioc 重新生成，改硬件配置在 CubeMX GUI 中改后点击 GENERATE CODE，不要在生成代码里手改初始化。生成文件中只允许在 USER CODE BEGIN/END 注释间填内容，业务逻辑集中在 app_tasks.c、scan.c、bt_uart.c、bt_protocol.c 等自定义文件。重新生成后需检查：① app_tasks.c/scan.c/bt_*.c 仍在 Keil 工程中；② stm32f4xx_it.c 无重复/空的 ISR；③ 完整编译达到 0 错误 0 警告。.ioc 需随代码提交。

## License

Copyright (c) 2026 STMicroelectronics. All rights reserved.