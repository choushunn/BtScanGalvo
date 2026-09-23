/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_tasks.h
  * @brief   应用 RTOS 任务层接口：创建 BTTask / ScanCtrlTask 与命令队列
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "cmsis_os.h"

extern osThreadId_t       BTTaskHandle;
extern osThreadId_t       ScanCtrlTaskHandle;
extern osMessageQueueId_t CmdQueueHandle;

/* 创建命令队列与两个应用任务（须在 osKernelInitialize 之后、osKernelStart 之前调用） */
void App_Tasks_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TASKS_H */