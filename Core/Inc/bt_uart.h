/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bt_uart.h
  * @brief   USART6 中断接收环形缓冲：供蓝牙透传(HC-05) 接收
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __BT_UART_H
#define __BT_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define BT_UART_RX_BUF 64u   /* 环形缓冲深度 */

void BT_UART_Init(void);              /* 配置 NVIC 并启动接收中断 */
uint8_t BT_UART_Get(uint8_t *byte);   /* 从环形缓冲取一字节，成功返回1 */
void BT_UART_Send(const uint8_t *data, uint8_t len); /* 阻塞发送 */

#ifdef __cplusplus
}
#endif

#endif /* __BT_UART_H */