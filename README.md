# BtScanGalvo 振镜扫描控制项目

本项目包含两个部分：

- **Firmware/** — STM32F407 + FreeRTOS 振镜（Galvo）扫描控制固件，通过 SPI 控制 DAC8563 驱动双轴振镜。详见 [Firmware/README.md](Firmware/README.md)。
- **APP/** — 蓝牙控制 APP，通过蓝牙（HC-05, USART6）与固件通信，用于切换形状、调节扫描幅度、启停/暂停扫描、调节点间隔等。命令协议见 [Firmware/README.md](Firmware/README.md) 的《蓝牙命令协议》。

## License

本项目（README 及 [APP/](APP/README.md)）遵循 **MIT 协议**，见 [LICENSE](LICENSE)。固件目录 `Firmware/` 保留其原有版权声明（Copyright (c) 2026 STMicroelectronics. All rights reserved.），不受本 MIT 协议约束。