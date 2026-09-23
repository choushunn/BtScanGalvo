/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* ---- DAC8563 命令（24位帧 = 8位命令/地址 + 16位数据，命名沿用官方例程） ---- */
#define CMD_SETA_UPDATEA      0x18u   /* 写 DAC-A 输入寄存器并更新 DAC-A */
#define CMD_SETB_UPDATEB      0x19u   /* 写 DAC-B 输入寄存器并更新 DAC-B */
#define CMD_GAIN              0x02u   /* 增益寄存器 */
#define CMD_PWR_UP_A_B        0x20u   /* 上电/掉电控制 */
#define CMD_RESET_ALL_REG     0x28u   /* 软件复位 */
#define CMD_LDAC_DIS          0x30u   /* LDAC 寄存器：设为同步更新，LDAC(LD/PD2) 引脚高低都不影响 */
#define CMD_INTERNAL_REF_EN   0x38u   /* 使能内部基准 */

#define DATA_RESET_ALL_REG    0x0001u
#define DATA_PWR_UP_A_B       0x0003u
#define DATA_INTERNAL_REF_EN  0x0001u
#define DATA_GAIN_B2_A2       0x0000u /* A/B 增益均为 2 → 0~5V（官方例程用 B2_A1） */
#define DATA_LDAC_NAB         0x0003u /* A/B 均由内部时钟更新，LDAC 引脚不起作用 */

/* ---- 振镜扫描形状（X → DAC-A，Y → DAC-B） ---- */
#define SINE_LUT_SIZE        255u    /* 正弦查表点数（复用官方例程的正弦表） */
#define GALVO_MAX_POINTS     512u    /* 形状点表容量 */
#define GALVO_POINT_DELAY_US 30u     /* 相邻两点的间隔(us)：256点≈110Hz重绘，人眼看是完整形状 */
#define GALVO_AMP_PERCENT    100u    /* 幅度：满量程百分比（100 = 0~5V 满幅） */

#define SHAPE_SQUARE         0u      /* KEY1(PD3) -> 正方形 */
#define SHAPE_RECT           1u      /* KEY2(PD4) -> 长方形（宽:高 = 2:1） */
#define SHAPE_CIRCLE         2u      /* KEY3(PD5) -> 圆形   */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* 正弦查表：官方例程的 255 点正弦表（0~2π，中心 32768），形状生成时当三角函数表用 */
static const uint16_t SineWave_Value[SINE_LUT_SIZE] = {
  32967,33775,34581,35387,36190,36992,37791,38588,39380,40169,
  40953,41732,42506,43274,44035,44790,45537,46277,47008,47731,
  48444,49148,49842,50526,51199,51861,52511,53149,53775,54388,
  54988,55575,56147,56706,57250,57779,58292,58791,59273,59740,
  60190,60623,61040,61439,61821,62185,62532,62860,63170,63462,
  63735,63989,64224,64440,64637,64815,64973,65112,65230,65330,
  65409,65469,65509,65528,65528,65509,65469,65409,65330,65230,
  65112,64973,64815,64637,64440,64224,63989,63735,63462,63170,
  62860,62532,62185,61821,61439,61040,60623,60190,59740,59273,
  58791,58292,57779,57250,56706,56147,55575,54988,54388,53775,
  53149,52511,51861,51199,50526,49842,49148,48444,47731,47008,
  46277,45537,44790,44035,43274,42506,41732,40953,40169,39380,
  38588,37791,36992,36190,35387,34581,33775,32967,32160,31353,
  30547,29742,28939,28139,27341,26547,25756,24969,24188,23411,
  22640,21876,21118,20367,19623,18888,18161,17443,16734,16034,
  15345,14667,14000,13344,12699,12067,11448,10841,10248,9668,
  9103,8552,8015,7494,6988,6497,6022,5564,5122,4697,
  4289,3899,3526,3170,2833,2514,2213,1930,1667,1422,
  1196,990,802,635,486,358,248,159,90,40,
  10,0,10,40,90,159,248,358,486,635,
  802,990,1196,1422,1667,1930,2213,2514,2833,3170,
  3526,3899,4289,4697,5122,5564,6022,6497,6988,7494,
  8015,8552,9103,9668,10248,10841,11448,12067,12699,13344,
  14000,14667,15345,16034,16734,17443,18161,18888,19623,20367,
  21118,21876,22640,23411,24188,24969,25756,26547,27341,28139,
  28939,29742,30547,31353,32160
};
static uint16_t shape_x[GALVO_MAX_POINTS];   /* 当前形状的 X 通道码值 */
static uint16_t shape_y[GALVO_MAX_POINTS];   /* 当前形状的 Y 通道码值 */
static uint16_t shape_len = 0u;              /* 当前形状点数 */
static uint16_t shape_pt  = 0u;              /* 正在输出的点序号 */
static uint8_t  shape_id  = SHAPE_CIRCLE;    /* 当前形状（上电默认圆） */
static GPIO_PinState key_prev[3] = {GPIO_PIN_SET, GPIO_PIN_SET, GPIO_PIN_SET}; /* 按键上次电平 */
static uint32_t led_tick  = 0u;              /* 状态灯计时 */
static uint8_t  led_state = 0u;              /* 状态灯当前亮灭 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  查表正弦/余弦（复用现成的正弦表，省掉浮点三角函数库）
  * @param  idx : 角度索引，0~254 对应 0~2π
  */
static float sine_lut(uint16_t idx)
{
  return ((float)SineWave_Value[idx % SINE_LUT_SIZE] - 32768.0f) / 32767.0f;
}

static float cosine_lut(uint16_t idx)
{
  return sine_lut((uint16_t)((idx + 64u) % SINE_LUT_SIZE));   /* sin(x+90°)=cos(x)，64 步≈90° */
}

/**
  * @brief  DWT 周期计数器初始化 + 微秒级延时
  *         只用于控制扫描点间隔，不占定时器，也不影响 HAL_Delay/HAL_GetTick
  */
static void Delay_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0u;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us * (SystemCoreClock / 1000000u);

  while ((DWT->CYCCNT - start) < ticks)
  {
  }
}

/**
  * @brief  生成形状点表（X/Y 都以中值 32768 为中心，幅度由 GALVO_AMP_PERCENT 决定）
  * @param  shape : 形状编号（SHAPE_SQUARE / SHAPE_RECT / SHAPE_CIRCLE）
  * @retval 该形状的点数
  */
static uint16_t Shape_Generate(uint8_t shape)
{
  uint32_t amp = (uint32_t)32767u * GALVO_AMP_PERCENT / 100u;   /* 半摆幅（码值） */
  uint16_t i;
  uint16_t n = 0u;

  switch (shape)
  {
    case SHAPE_CIRCLE:                     /* 圆：256 点，角度均匀分布 */
      n = 256u;
      for (i = 0u; i < n; i++)
      {
        uint16_t k = (uint16_t)((uint32_t)i * SINE_LUT_SIZE / n);
        shape_x[i] = (uint16_t)(32768 + (int32_t)((float)amp * cosine_lut(k)));
        shape_y[i] = (uint16_t)(32768 + (int32_t)((float)amp * sine_lut(k)));
      }
      break;

    case SHAPE_SQUARE:                     /* 正方形：四边各 64 点，右上→右下→左下→左上 */
    case SHAPE_RECT:                       /* 长方形：宽:高 = 2:1，点序与正方形一致 */
    {
      uint32_t hw = amp;                                         /* 半宽 */
      uint32_t hh = (shape == SHAPE_SQUARE) ? amp : (amp / 2u);  /* 半高 */
      uint32_t sx = 2u * hw / 64u;                               /* 水平边每点步进 */
      uint32_t sy = 2u * hh / 64u;                               /* 垂直边每点步进 */

      n = 256u;
      for (i = 0u; i < n; i++)
      {
        uint16_t side = (uint16_t)(i / 64u);
        uint32_t t    = (uint32_t)(i % 64u);
        int32_t  dx   = 0;
        int32_t  dy   = 0;

        switch (side)
        {
          case 0u:  dx =  (int32_t)hw;                  dy =  (int32_t)hh - (int32_t)(sy * t);  break;
          case 1u:  dx =  (int32_t)hw - (int32_t)(sx * t); dy = -(int32_t)hh;                    break;
          case 2u:  dx = -(int32_t)hw;                  dy = -(int32_t)hh + (int32_t)(sy * t);  break;
          default:  dx = -(int32_t)hw + (int32_t)(sx * t); dy =  (int32_t)hh;                    break;
        }
        shape_x[i] = (uint16_t)(32768 + dx);
        shape_y[i] = (uint16_t)(32768 + dy);
      }
      break;
    }

    default:                               /* SHAPE_CIRCLE：圆形，256 点，角度均匀分布 */
      n = 256u;
      for (i = 0u; i < n; i++)
      {
        uint16_t k = (uint16_t)((uint32_t)i * SINE_LUT_SIZE / n);
        shape_x[i] = (uint16_t)(32768 + (int32_t)((float)amp * cosine_lut(k)));
        shape_y[i] = (uint16_t)(32768 + (int32_t)((float)amp * sine_lut(k)));
      }
      break;
  }

  return n;
}

/**
  * @brief  向 DAC8563 写一帧：8 位命令/地址 + 16 位数据（共 24 个 SCLK，MSB 在前）
  * @param  cmd  : DB23-16（高 2 位无关，其后 3 位命令 + 3 位通道地址）
  * @param  data : DB15-0
  */
static void DAC8563_Write(uint8_t cmd, uint16_t data)
{
  uint8_t tx[3] = { cmd, (uint8_t)(data >> 8), (uint8_t)data };

  HAL_GPIO_WritePin(SYN_GPIO_Port, SYN_Pin, GPIO_PIN_RESET);  /* SYNC 拉低，使能输入移位寄存器 */
  HAL_SPI_Transmit(&hspi3, tx, 3, 10);                        /* 24 个 SCLK */
  HAL_GPIO_WritePin(SYN_GPIO_Port, SYN_Pin, GPIO_PIN_SET);    /* SYNC 拉高，第 24 个下降沿已更新输出 */
}

/**
  * @brief  同时更新 A、B 两路输出（对应官方例程 DAC_OutAB）
  *         用的是"写输入寄存器并更新该通道"命令（0x18/0x19），自带更新动作，
  *         不依赖 LDAC 引脚；初始化时还额外把 LDAC 寄存器设成同步模式做双保险
  */
static void DAC_OutAB(uint16_t data_a, uint16_t data_b)
{
  DAC8563_Write(CMD_SETA_UPDATEA, data_a);
  DAC8563_Write(CMD_SETB_UPDATEB, data_b);
}

/**
  * @brief  状态灯任务：只保留 LED1(PB10) 作运行指示，每 500ms 翻转一次（非阻塞）
  */
static void Status_LED_Task(void)
{
  if ((HAL_GetTick() - led_tick) >= 500u)
  {
    led_tick  = HAL_GetTick();
    led_state = (uint8_t)(led_state ^ 1u);
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, led_state ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
}

/**
  * @brief  把其余 4 个 LED 全部熄灭（只留 LED1 当状态灯）
  */
static void LED_Off_Others(void)
{
  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED7_GPIO_Port, LED7_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED8_GPIO_Port, LED8_Pin, GPIO_PIN_RESET);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SPI3_Init();
  MX_USART1_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(Light_PA6_GPIO_Port, Light_PA6_Pin, GPIO_PIN_SET);    /* 激光/光源关闭（低电平点亮，所以置高=关） */

  /* ---- DAC8563 初始化（顺序仿官方例程 DAC8563_Init） ---- */
  HAL_GPIO_WritePin(CLR_GPIO_Port, CLR_Pin, GPIO_PIN_SET);      /* CLR 置高，不触发异步清除 */
  HAL_GPIO_WritePin(SYN_GPIO_Port, SYN_Pin, GPIO_PIN_SET);      /* SYNC 空闲为高 */
  DAC8563_Write(CMD_RESET_ALL_REG,    DATA_RESET_ALL_REG);      /* 复位所有寄存器 */
  DAC8563_Write(CMD_PWR_UP_A_B,       DATA_PWR_UP_A_B);         /* 上电 DAC-A、DAC-B */
  DAC8563_Write(CMD_INTERNAL_REF_EN,  DATA_INTERNAL_REF_EN);    /* 使能内部 2.5V 基准，增益=2 */
  DAC8563_Write(CMD_GAIN,             DATA_GAIN_B2_A2);         /* A/B 增益 = 2 → 0~5V */
  DAC8563_Write(CMD_LDAC_DIS,         DATA_LDAC_NAB);           /* 双通道同步更新，不依赖 LDAC 引脚 */
  DAC_OutAB(32768u, 32768u);                                    /* 光点先停在中心 */

  /* ---- 振镜形状初始化 ---- */
  Delay_Init();                                                 /* DWT 微秒延时（控制扫描点间隔） */
  shape_len = Shape_Generate(shape_id);                         /* 上电默认圆 */
  shape_pt  = 0u;
  LED_Off_Others();                                             /* 只保留 LED1 作状态灯 */
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ---- 按键直接选形状（按下沿生效）：KEY1 正方形 / KEY2 长方形 / KEY3 圆形 ---- */
    GPIO_PinState k1 = HAL_GPIO_ReadPin(PD3_GPIO_Port, PD3_Pin);
    GPIO_PinState k2 = HAL_GPIO_ReadPin(PD4_GPIO_Port, PD4_Pin);
    GPIO_PinState k3 = HAL_GPIO_ReadPin(PD5_GPIO_Port, PD5_Pin);
    uint8_t       new_shape = shape_id;

    if ((key_prev[0] == GPIO_PIN_SET) && (k1 == GPIO_PIN_RESET)) new_shape = SHAPE_SQUARE;
    if ((key_prev[1] == GPIO_PIN_SET) && (k2 == GPIO_PIN_RESET)) new_shape = SHAPE_RECT;
    if ((key_prev[2] == GPIO_PIN_SET) && (k3 == GPIO_PIN_RESET)) new_shape = SHAPE_CIRCLE;

    key_prev[0] = k1;
    key_prev[1] = k2;
    key_prev[2] = k3;

    if (new_shape != shape_id)               /* 换形状：重新生成点表，从头开始画 */
    {
      shape_id  = new_shape;
      shape_len = Shape_Generate(shape_id);
      shape_pt  = 0u;
    }

    /* ---- 输出当前形状的下一个点：X → DAC-A，Y → DAC-B ---- */
    DAC_OutAB(shape_x[shape_pt], shape_y[shape_pt]);
    shape_pt++;
    if (shape_pt >= shape_len) shape_pt = 0u;

    Status_LED_Task();                            /* LED1 运行指示灯：500ms 闪烁（非阻塞） */
    delay_us(GALVO_POINT_DELAY_US);               /* 控制重绘速度：约110Hz，人眼看是完整形状 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
