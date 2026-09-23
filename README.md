# OCT_CT

STM32F407 振镜（Galvo）扫描控制固件，用于 OCT/CT 扫描光学系统的振镜驱动与控制。

## 项目简介

本项目基于 **STM32F407VETx**（HAL 库 + FreeRTOS），通过 **SPI** 控制 **DAC8563** 双通道 DAC，驱动双轴振镜（X 轴 → DAC-A，Y 轴 → DAC-B），以查找表方式在振镜控制平面上绘制预设形状，实现激光扫描。

## 硬件与外设

- 主控：STM32F407VETx
- 系统：FreeRTOS（CMSIS-OS v2）
- 外设使用：SPI3+DMA（DAC8563，SPI3_TX = **DMA1_Stream5/CH0**）、USART1（调试打印）、USART6（蓝牙 HC-05）、GPIO、TIM6（扫描点步进）、TIM7（系统节拍）
- 显示：DAC8563 内部基准使能，A/B 增益 ×2 → 输出 0~5V 满幅
- TIM6 与 SPI3_TX DMA 的初始化由 **STM32CubeMX 生成**（`MX_TIM6_Init`、`MX_SPI3_Init` 内联），重新生成工程时不会丢失。

## 架构

扫描点输出走硬件热路径，CPU 不参与逐点时序；低速、阻塞、多变的控制逻辑交给 RTOS 任务，两者通过命令队列解耦。

- 硬件热路径：TIM6 定住点间隔，TIM6 更新中断经 `HAL_TIM_PeriodElapsedCallback` 调 `Scan_TimerTick()` 拉低 SYNC 并触发 SPI3_TX DMA 发出当前点 X 帧（24bit），DMA 完成中断（`DMA1_Stream5`）翻转 SYNC 锁存 X 并触发 Y 帧，再翻转 SYNC 锁存 Y，推进到下一个点。数据/时序全部脱 CPU。
- 应用层 RTOS 逻辑集中在自定义文件 **app_tasks.c**（BTTask + ScanCtrlTask + 命令队列），CubeMX 生成的 freertos.c 仅在 USER CODE 钩子中调用 `App_Tasks_Init()`，因此重新生成工程不会覆盖任务代码。
- BTTask：从 USART6 环形缓冲收字节 → 组帧 → 解析 → 投递命令到命令队列并回 ACK。
- ScanCtrlTask：消费命令队列，在“安全点”停硬件 → 重建点表/重编码帧缓冲 → 重启硬件；同时轮询 KEY1/2/3 走同一控制路径。
- 状态变更隔离：点表与帧缓冲为单缓冲，仅在“停硬件”后由 ScanCtrlTask 写入，ISR 只读已提交的表，避免撕裂。

## 蓝牙命令协议（HC-05 透传）

帧格式：`[SOF 0xAA][LEN=Payload长度][CMD][Payload(LEN)][CRC8]`，CRC8 覆盖 CMD+Payload。

| CMD | 含义 | Payload |
|-----|------|---------|
| 0x01 | 切换形状 | `shape`（0 正方形/1 长方形/2 圆形） |
| 0x02 | 调扫描幅度 | `0~100`（满量程百分比） |
| 0x03 | 扫描启停 | `0` 停止 / `1` 启动 / `2` 暂停 |
| 0x04 | 调点间隔 | `interval_us`（小端 uint16，≥5us） |
| 0xF0 | ACK 回执 | `err`（0 正常 / 1 未知命令） |

命令字 `0x80` 及以上保留给后续扩展。

## 性能边界

- 点表上限 4096 点/帧，编码 6B/点；间隔下限 5us（约 200k 点/秒，ISR 方案 CPU 占用约 15~25%）。
- 追求 2~4us 点间隔需升级为纯硬件 SYNC 门控（本方案仅预留接口）。

## 功能说明

- 通过 255 点正弦查找表生成 X/Y 波形，实时计算振镜形状点（最多 4096 点）
- 按键切换绘制形状：KEY1(PD3) 正方形、KEY2(PD4) 长方形（宽:高=2:1）、KEY3(PD5) 圆形
- 蓝牙（HC-05, USART6, 115200）可切换形状、调幅度、启停/暂停、调点间隔
- 相邻点间隔默认 30us，可通过蓝牙在线修改
- 幅度可调（满量程百分比，默认 100%）

## 目录结构

```
OCT_CT/
├── Core/          用户代码（含 app_tasks.c 应用任务、scan.c 扫描模块、bt_*.c 蓝牙协议、main、外设驱动）
├── Drivers/       ST 官方 HAL 驱动
├── Middlewares/   FreeRTOS 中间件
├── MDK-ARM/       Keil MDK 工程（BtScanGalvo.uvprojx）
└── BtScanGalvo.ioc  STM32CubeMX 工程文件
```

## 构建

使用 Keil MDK-ARM 打开 `MDK-ARM/BtScanGalvo.uvprojx`，编译后通过 ST-Link 烧录。硬件配置由 `BtScanGalvo.ioc` 生成（STM32CubeMX 可重新生成工程）；自定义文件 `app_tasks.c`/`scan.c` 等与 USER CODE 段在重新生成后保持。

## License

Copyright (c) 2026 STMicroelectronics. All rights reserved.