/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_tasks.c
  * @brief   应用 RTOS 任务层：蓝牙控制任务(BTTask) + 扫描控制任务(ScanCtrlTask)
  *          命令经 FreeRTOS 消息队列在两者间传递。
  *          本文件为自定义文件，不会被 CubeMX 重新生成覆盖，由其生成的
  *          freertos.c 在 MX_FREERTOS_Init 的 USER CODE 中调用 App_Tasks_Init。
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "app_tasks.h"
#include "bt_protocol.h"
#include "bt_uart.h"
#include "scan.h"

/* Private typedef -----------------------------------------------------------*/
typedef struct {
  uint8_t  cmd;
  uint16_t value;
} AppCmd;

/* Private define ------------------------------------------------------------*/
#define CMD_QUEUE_LEN 8u

/* Private variables ---------------------------------------------------------*/
osThreadId_t       BTTaskHandle;
osThreadId_t       ScanCtrlTaskHandle;
osMessageQueueId_t CmdQueueHandle;

/* Definitions for BTTask */
const osThreadAttr_t BTTask_attributes = {
  .name = "BTTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Definitions for ScanCtrlTask */
const osThreadAttr_t ScanCtrlTask_attributes = {
  .name = "ScanCtrl",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
static void StartBTTask(void *argument);
static void StartScanCtrlTask(void *argument);

/* 回应一帧 ACK 回执到手机端 */
static void BT_Reply(uint8_t err)
{
  uint8_t buf[4];
  uint8_t n = BT_BuildFrame(BT_CMD_ACK, &err, 1u, buf);
  BT_UART_Send(buf, n);
}

/**
  * @brief 蓝牙/上位机控制任务：
  *        从 USART6 环形缓冲取字节 -> 组帧 -> 解析 -> 投递命令到 CmdQueue
  */
static void StartBTTask(void *argument)
{
  BT_UART_Init();
  for(;;)
  {
    uint8_t b;
    BT_Frame f;
    AppCmd  c;
    uint8_t reply = 0u;

    if (BT_UART_Get(&b) && BT_ParseByte(b, &f))
    {
      c.cmd = 0u;
      c.value = 0u;
      switch (f.cmd)
      {
        case BT_CMD_SWITCH_SHAPE:
          c.cmd = f.cmd; c.value = f.payload[0];
          if (osMessageQueuePut(CmdQueueHandle, &c, 0u, 0u) == osOK) reply = 1u;
          break;

        case BT_CMD_SET_AMPLITUDE:
          c.cmd = f.cmd; c.value = f.payload[0];
          if (osMessageQueuePut(CmdQueueHandle, &c, 0u, 0u) == osOK) reply = 1u;
          break;

        case BT_CMD_SET_SCAN_STATE:
          c.cmd = f.cmd; c.value = f.payload[0];
          if (osMessageQueuePut(CmdQueueHandle, &c, 0u, 0u) == osOK) reply = 1u;
          break;

        case BT_CMD_SET_INTERVAL:
          c.cmd = f.cmd;
          c.value = (uint16_t)(((uint16_t)f.payload[1] << 8u) | f.payload[0]);
          if (osMessageQueuePut(CmdQueueHandle, &c, 0u, 0u) == osOK) reply = 1u;
          break;

        default:
          BT_Reply(BT_ERR_CMD);   /* 未知命令回错误 */
          reply = 0u;
          break;
      }
      if (reply) BT_Reply(0u);
    }
    osDelay(1);
  }
}

/**
  * @brief 扫描控制任务：消费命令队列，安全地驱动扫描模块；
  *        KEY1/2/3 按键切换形状也在此统一处理（与蓝牙同路径）
  */
static void StartScanCtrlTask(void *argument)
{
  /* 按键上次电平（PD3/PD4/PD5），用于下降沿检测 */
  GPIO_PinState key_prev[3] = {GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET};

  for(;;)
  {
    AppCmd c;

    /* 1) 处理蓝牙/上位机下发的命令（10ms 轮询） */
    if (osMessageQueueGet(CmdQueueHandle, &c, NULL, 10u) == osOK)
    {
      switch (c.cmd)
      {
        case BT_CMD_SWITCH_SHAPE:
          Scan_SetShape((uint8_t)c.value);
          break;
        case BT_CMD_SET_AMPLITUDE:
          Scan_SetAmplitude((uint8_t)c.value);
          break;
        case BT_CMD_SET_SCAN_STATE:
          switch (c.value)
          {
            case 0u: Scan_Stop();  break;   /* 停止 */
            case 1u: Scan_Start(); break;   /* 启动 */
            case 2u: Scan_Pause(); break;   /* 暂停 */
            default: break;
          }
          break;
        case BT_CMD_SET_INTERVAL:
          Scan_SetIntervalUs((uint32_t)c.value);
          break;
        default:
          break;
      }
    }

    /* 2) 按键切换形状（KEY1 正方 / KEY2 长方 / KEY3 圆） */
    GPIO_PinState k1 = HAL_GPIO_ReadPin(PD3_GPIO_Port, PD3_Pin);
    GPIO_PinState k2 = HAL_GPIO_ReadPin(PD4_GPIO_Port, PD4_Pin);
    GPIO_PinState k3 = HAL_GPIO_ReadPin(PD5_GPIO_Port, PD5_Pin);

    if ((key_prev[0] == GPIO_PIN_SET) && (k1 == GPIO_PIN_RESET)) Scan_SetShape(SCAN_SHAPE_SQUARE);
    if ((key_prev[1] == GPIO_PIN_SET) && (k2 == GPIO_PIN_RESET)) Scan_SetShape(SCAN_SHAPE_RECT);
    if ((key_prev[2] == GPIO_PIN_SET) && (k3 == GPIO_PIN_RESET)) Scan_SetShape(SCAN_SHAPE_CIRCLE);

    key_prev[0] = k1;
    key_prev[1] = k2;
    key_prev[2] = k3;
  }
}

/**
  * @brief 创建命令队列与两个应用任务（由 freertos.c 的 MX_FREERTOS_Init 调用）
  * @note  必须在 osKernelInitialize 之后、osKernelStart 之前执行
  */
void App_Tasks_Init(void)
{
  CmdQueueHandle = osMessageQueueNew(CMD_QUEUE_LEN, sizeof(AppCmd), NULL);

  BTTaskHandle = osThreadNew(StartBTTask, NULL, &BTTask_attributes);
  ScanCtrlTaskHandle = osThreadNew(StartScanCtrlTask, NULL, &ScanCtrlTask_attributes);
}