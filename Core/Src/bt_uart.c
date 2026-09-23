/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bt_uart.c
  * @brief   USART6 中断接收环形缓冲实现（蓝牙 HC-05 透传串口）
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "bt_uart.h"
#include "usart.h"

static volatile uint8_t  s_ring[BT_UART_RX_BUF];
static volatile uint16_t s_head = 0u;  /* 写入位置(ISR) */
static volatile uint16_t s_tail = 0u;  /* 读取位置(任务) */
static uint8_t           s_rx_byte;    /* HAL 单字节接收暂存 */

void BT_UART_Init(void)
{
  /* USART6 已在 MX_USART6_UART_Init 配置为 115200，这里只开中断接收 */
  HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART6_IRQn);
  HAL_UART_Receive_IT(&huart6, &s_rx_byte, 1u);
}

uint8_t BT_UART_Get(uint8_t *byte)
{
  if (s_tail == s_head) return 0u;
  *byte = (uint8_t)s_ring[s_tail];
  s_tail++;
  if (s_tail >= BT_UART_RX_BUF) s_tail = 0u;
  return 1u;
}

void BT_UART_Send(const uint8_t *data, uint8_t len)
{
  HAL_UART_Transmit(&huart6, (uint8_t *)data, len, 100u);
}

/* USART6 接收中断 */
void USART6_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart6);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart != &huart6) return;
  s_ring[s_head] = s_rx_byte;
  s_head++;
  if (s_head >= BT_UART_RX_BUF) s_head = 0u;
  HAL_UART_Receive_IT(&huart6, &s_rx_byte, 1u); /* 重新接收下一个字节 */
}