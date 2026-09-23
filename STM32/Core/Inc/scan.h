/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scan.h
  * @brief   振镜扫描模块：高速点输出（TIM6 + SPI3_DMA -> DAC8563）
  *          热路径由硬件驱动，RTOS 任务只负责"安全地"重建点表/启停/改参数
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __SCAN_H
#define __SCAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* 点表规模上限（每点 6 字节编码） */
#define SCAN_MAX_POINTS   4096u
#define SCAN_FRAME_BYTES  6u    /* {cmdX, X_hi, X_lo, cmdY, Y_hi, Y_lo} */

/* 形状编号（与旧 main.c 的 SHAPE_* 一致） */
#define SCAN_SHAPE_SQUARE 0u
#define SCAN_SHAPE_RECT   1u
#define SCAN_SHAPE_CIRCLE 2u

/* 参数默认值 */
#define SCAN_DEFAULT_INTERVAL_US 30u
#define SCAN_DEFAULT_AMP_PERCENT 100u
#define SCAN_MIN_INTERVAL_US    5u   /* 点间隔下限，ISR 翻转 SYNC 的可靠工作点 */

void Scan_Init(void);                 /* 初始化 DAC/GPIO/TIM6/DMA/NVIC，不启动 */
void Scan_Start(void);                /* 开始/恢复逐步输出 */
void Scan_Stop(void);                 /* 完全停止，DAC 回中点 */
void Scan_Pause(void);                /* 暂停（保留点表，不输出） */

void Scan_SetShape(uint8_t shape);    /* SCAN_SHAPE_* */
void Scan_CycleShape(void);           /* 面板按键：循环切换 圆→正方→长方→圆 */
void Scan_SetAmplitude(uint8_t percent); /* 0..100 */
uint8_t Scan_GetAmplitude(void);      /* 当前幅度百分比（面板 +10% 步进用） */
void Scan_SetIntervalUs(uint32_t us);    /* >=5us */

uint8_t Scan_IsIdle(void);            /* 返回当前是否处于停止/暂停态 */

void Scan_TimerTick(void);            /* TIM6 更新中断 -> 推进一点（由 main.c 的
                                         HAL_TIM_PeriodElapsedCallback 调起） */

#ifdef __cplusplus
}
#endif

#endif /* __SCAN_H */
