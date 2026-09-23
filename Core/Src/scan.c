/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    scan.c
  * @brief   振镜扫描模块实现（高速热路径）
  *
  *  硬件热路径：TIM6 定住点间隔 -> 每点 ISR 拉低 SYNC 并触发 SPI3_TX DMA 发 X 帧
  *            -> DMA 完成翻转 SYNC -> 触发 Y 帧 -> 完成再次翻转 SYNC -> 推进点序号。
  *  CPU 不参与逐点时序；状态变更（形状/幅度/间隔/启停）一律在应用层先停硬件、
  *  重建点表、再重启动，保证 ISR 只读到"已提交"的点表（单缓冲 + 停硬件期写入）。
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "scan.h"
#include "spi.h"
#include "tim.h"   /* htim6 由 CubeMX 生成（tim.c/tim.h） */

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN PV */

/* DAC8563 命令（24 位帧 = 8 位命令/地址 + 16 位数据，沿用官方例程语义） */
#define CMD_SETA_UPDATEA     0x18u
#define CMD_SETB_UPDATEB     0x19u
#define CMD_GAIN             0x02u
#define CMD_PWR_UP_A_B       0x20u
#define CMD_RESET_ALL_REG    0x28u
#define CMD_LDAC_DIS         0x30u
#define CMD_INTERNAL_REF_EN  0x38u

#define DATA_RESET_ALL_REG   0x0001u
#define DATA_PWR_UP_A_B      0x0003u
#define DATA_INTERNAL_REF_EN 0x0001u
#define DATA_GAIN_B2_A2      0x0000u
#define DATA_LDAC_NAB        0x0003u

#define DAC_MID              32768u   /* 中点码值 */
#define SINE_LUT_SIZE        255u

#define SCAN_PIN_SYNC_LOW()  HAL_GPIO_WritePin(SYN_GPIO_Port, SYN_Pin, GPIO_PIN_RESET)
#define SCAN_PIN_SYNC_HIGH() HAL_GPIO_WritePin(SYN_GPIO_Port, SYN_Pin, GPIO_PIN_SET)

/* 正弦查表（0~2π 中心 32768），形状生成当三角函数表用 */
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

/* 当前参数 */
static uint8_t  s_shape_id  = SCAN_SHAPE_CIRCLE;
static uint8_t  s_amp_pct   = SCAN_DEFAULT_AMP_PERCENT;
static uint32_t s_interval_us = SCAN_DEFAULT_INTERVAL_US;
static volatile uint8_t s_running = 0u;   /* 0=停止/暂停，1=正在逐步输出 */

/* 点表与编码帧缓冲（单缓冲；仅在"停硬件"后的重构建期间被写，ISR 只读） */
static uint16_t s_x[SCAN_MAX_POINTS];
static uint16_t s_y[SCAN_MAX_POINTS];
static uint8_t  s_frame[SCAN_MAX_POINTS * SCAN_FRAME_BYTES];

/* ISR 共享的运行状态 */
static volatile uint16_t s_pt_count = 0u;  /* 当前点数（=帧字节/6） */
static volatile uint16_t s_pt_index = 0u;  /* 正在输出的点序号 */
static volatile uint8_t  s_dma_stage = 0u; /* 0=当前帧为 X，1=Y */

/* USER CODE END PV */

/* USER CODE BEGIN PFP */
static float    sine_lut(uint16_t idx);
static float    cosine_lut(uint16_t idx);
static uint16_t Shape_Generate(uint8_t shape, uint8_t amp_percent,
                               uint16_t *dst_x, uint16_t *dst_y);
static void     EncodeToFrame(uint8_t *buf, const uint16_t *x,
                              const uint16_t *y, uint16_t n);
static void     DAC8563_Write(uint8_t cmd, uint16_t data);
static void     DAC_OutCenter(void);
static void     Scan_HW_Stop(void);
static void     Scan_HW_Start(void);
static void     Scan_Rebuild(void);
static uint32_t Scan_TimerSetInterval(uint32_t us);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */
static float sine_lut(uint16_t idx)
{
  return ((float)SineWave_Value[idx % SINE_LUT_SIZE] - 32768.0f) / 32767.0f;
}

static float cosine_lut(uint16_t idx)
{
  return sine_lut((uint16_t)((idx + 64u) % SINE_LUT_SIZE)); /* sin(x+90°)=cos(x) */
}

/* 生成形状点表（X/Y 以 DAC_MID 为中心，幅度由 amp_percent 决定） */
static uint16_t Shape_Generate(uint8_t shape, uint8_t amp_percent,
                               uint16_t *dst_x, uint16_t *dst_y)
{
  uint32_t amp = (uint32_t)32767u * amp_percent / 100u; /* 半摆幅（码值） */
  uint16_t i;
  uint16_t n = 0u;

  switch (shape)
  {
    case SCAN_SHAPE_CIRCLE:                /* 圆：256 点，角度均匀分布 */
      n = 256u;
      for (i = 0u; i < n; i++)
      {
        uint16_t k = (uint16_t)((uint32_t)i * SINE_LUT_SIZE / n);
        dst_x[i] = (uint16_t)(32768 + (int32_t)((float)amp * cosine_lut(k)));
        dst_y[i] = (uint16_t)(32768 + (int32_t)((float)amp * sine_lut(k)));
      }
      break;

    case SCAN_SHAPE_SQUARE:               /* 正方形：四边各 64 点 */
    case SCAN_SHAPE_RECT:                 /* 长方形：宽:高 = 2:1 */
    {
      uint32_t hw = amp;                /* 半宽 */
      uint32_t hh = (shape == SCAN_SHAPE_SQUARE) ? amp : (amp / 2u);
      uint32_t sx = 2u * hw / 64u;
      uint32_t sy = 2u * hh / 64u;

      n = 256u;
      for (i = 0u; i < n; i++)
      {
        uint16_t side = (uint16_t)(i / 64u);
        uint32_t t    = (uint32_t)(i % 64u);
        int32_t  dx   = 0;
        int32_t  dy   = 0;

        switch (side)
        {
          case 0u: dx =  (int32_t)hw;                  dy =  (int32_t)hh - (int32_t)(sy * t); break;
          case 1u: dx =  (int32_t)hw - (int32_t)(sx * t); dy = -(int32_t)hh;                    break;
          case 2u: dx = -(int32_t)hw;                  dy = -(int32_t)hh + (int32_t)(sy * t);  break;
          default: dx = -(int32_t)hw + (int32_t)(sx * t); dy =  (int32_t)hh;                    break;
        }
        dst_x[i] = (uint16_t)(32768 + dx);
        dst_y[i] = (uint16_t)(32768 + dy);
      }
      break;
    }

    default:                              /* 兜底：圆 */
      n = 256u;
      for (i = 0u; i < n; i++)
      {
        uint16_t k = (uint16_t)((uint32_t)i * SINE_LUT_SIZE / n);
        dst_x[i] = (uint16_t)(32768 + (int32_t)((float)amp * cosine_lut(k)));
        dst_y[i] = (uint16_t)(32768 + (int32_t)((float)amp * sine_lut(k)));
      }
      break;
  }
  return n;
}

/* 将点表编码为 DAC 帧缓冲：每点 {cmdX,Xh,Xl,cmdY,Yh,Yl} */
static void EncodeToFrame(uint8_t *buf, const uint16_t *x, const uint16_t *y, uint16_t n)
{
  uint16_t i;
  for (i = 0u; i < n; i++)
  {
    uint8_t *p = &buf[i * SCAN_FRAME_BYTES];
    p[0] = CMD_SETA_UPDATEA;
    p[1] = (uint8_t)(x[i] >> 8);
    p[2] = (uint8_t)x[i];
    p[3] = CMD_SETB_UPDATEB;
    p[4] = (uint8_t)(y[i] >> 8);
    p[5] = (uint8_t)y[i];
  }
}

/* 向 DAC8563 写一帧（阻塞，仅初始化/回中点位用） */
static void DAC8563_Write(uint8_t cmd, uint16_t data)
{
  uint8_t tx[3] = { cmd, (uint8_t)(data >> 8), (uint8_t)data };
  SCAN_PIN_SYNC_LOW();
  HAL_SPI_Transmit(&hspi3, tx, 3, 100);
  SCAN_PIN_SYNC_HIGH();
}

static void DAC_OutCenter(void)
{
  DAC8563_Write(CMD_SETA_UPDATEA, DAC_MID);
  DAC8563_Write(CMD_SETB_UPDATEB, DAC_MID);
}

/* 重建点表并编码到 s_frame（须在停硬件后调用） */
static void Scan_Rebuild(void)
{
  uint16_t n = Shape_Generate(s_shape_id, s_amp_pct, s_x, s_y);
  EncodeToFrame(s_frame, s_x, s_y, n);
  s_pt_count = n;
  s_pt_index = 0u;
  s_dma_stage = 0u;
}

/* 配置 TIM6 点间隔，返回实际生效的 us（含进位误差） */
static uint32_t Scan_TimerSetInterval(uint32_t us)
{
  uint32_t tick_hz = SystemCoreClock / 2u;          /* APB1=/4 且 TIM 时钟 x2 => HCLK/2 */
  uint32_t ticks    = (tick_hz / 1000000u) * us;    /* 该间隔对应的定时器计数值 */
  uint32_t psc = 0u;
  uint16_t arr;

  if (ticks > 65535u)
  {
    psc = ticks / 65535u;
    if (psc > 0xFFFFu) psc = 0xFFFFu;
  }
  arr = (uint16_t)((ticks / (psc + 1u)) - 1u);

  /* TIM6 直接寄存器操作（无 HAL 句柄，仅靠外设指针） */
  TIM6->PSC = psc;
  TIM6->ARR = arr;
  TIM6->EGR = TIM_EGR_UG;   /* 让新的 PSC/ARR 立即加载进影子寄存器 */

  return (uint32_t)((uint32_t)(arr + 1u) * (psc + 1u) * 1000000u / tick_hz);
}

/* 停止硬件热路径（只关断本模块所属的 TIM6 / DMA 中断，不关全局）
   注意：调用方在"重建点表"前必须调用本函数，重建过程较长（float），不能关全局中断 */
static void Scan_HW_Stop(void)
{
  /* 先按外设关断中断，并清 pending，避免切换期间 ISR 闯入/滞后触发 */
  NVIC_DisableIRQ(TIM6_DAC_IRQn);
  NVIC_DisableIRQ(DMA1_Stream5_IRQn);

  __HAL_TIM_DISABLE(&htim6);                       /* 停 TIM6（HAL 句柄） */
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
  NVIC_ClearPendingIRQ(TIM6_DAC_IRQn);

  if (hspi3.hdmatx != NULL)
  {
    __HAL_SPI_DISABLE(&hspi3);                      /* 先停 SPI */
    hspi3.Instance->CR2 &= ~SPI_CR2_TXDMAEN;
    __HAL_DMA_DISABLE(hspi3.hdmatx);
    /* 清 DMA1_Stream5 所有状态标志（高半段 HIFCR），避免遗留 TC/HT 造成误回调 */
    DMA1->HIFCR = DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 |
                  DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5;
    hspi3.State = HAL_SPI_STATE_READY;
    hspi3.Lock  = HAL_UNLOCKED;
    NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
  }

  SCAN_PIN_SYNC_HIGH();
}

/* 启动硬件热路径（重新使能 TIM6 / DMA 中断并起振） */
static void Scan_HW_Start(void)
{
  /* 清除更新标志、开更新中断并启动计数（HAL 宏，效果同原直接寄存器） */
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
  __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
  __HAL_TIM_ENABLE(&htim6);
  NVIC_ClearPendingIRQ(TIM6_DAC_IRQn);
  NVIC_ClearPendingIRQ(DMA1_Stream5_IRQn);
  NVIC_EnableIRQ(DMA1_Stream5_IRQn);
  NVIC_EnableIRQ(TIM6_DAC_IRQn);
}
/* USER CODE END 0 */

/* 对外接口 ---------------------------------------------------------------- */

void Scan_Init(void)
{
  /* ---- DAC8563 初始化 ---- */
  HAL_GPIO_WritePin(CLR_GPIO_Port, CLR_Pin, GPIO_PIN_SET);
  SCAN_PIN_SYNC_HIGH();
  DAC8563_Write(CMD_RESET_ALL_REG,   DATA_RESET_ALL_REG);
  DAC8563_Write(CMD_PWR_UP_A_B,      DATA_PWR_UP_A_B);
  DAC8563_Write(CMD_INTERNAL_REF_EN, DATA_INTERNAL_REF_EN);
  DAC8563_Write(CMD_GAIN,            DATA_GAIN_B2_A2);
  DAC8563_Write(CMD_LDAC_DIS,        DATA_LDAC_NAB);

  /* TIM6 已由 CubeMX 的 MX_TIM6_Init()（main.c 调用）配置为内部时钟基础定时器；
     SPI3_TX DMA 已由 MX_SPI3_Init() 配置为 DMA1_Stream5/CH0 并 LINK 到 hspi3.hdmatx。
     这里只需设置扫描点间隔（PSC/ARR），NVIC 已由生成代码开启，启停交给 Scan_HW_Start/Stop */
  Scan_TimerSetInterval(SCAN_DEFAULT_INTERVAL_US);

  /* ---- 初始点表 + 停止；激光关 ---- */
  HAL_GPIO_WritePin(Light_PA6_GPIO_Port, Light_PA6_Pin, GPIO_PIN_SET); /* 激光/光源关闭 */
  Scan_Rebuild();
  s_running = 0u;
  Scan_HW_Stop();
  DAC_OutCenter();
}

void Scan_Start(void)
{
  if (s_running) return;
  s_running = 1u;
  Scan_HW_Start();
}

void Scan_Stop(void)
{
  s_running = 0u;
  Scan_HW_Stop();
  DAC_OutCenter();
}

void Scan_Pause(void)
{
  s_running = 0u;
  Scan_HW_Stop();
}

void Scan_SetShape(uint8_t shape)
{
  if (shape > SCAN_SHAPE_CIRCLE) shape = SCAN_SHAPE_CIRCLE;
  if (s_shape_id == shape) return;
  s_shape_id = shape;
  Scan_HW_Stop();
  Scan_Rebuild();
  if (s_running) Scan_HW_Start();
}

void Scan_SetAmplitude(uint8_t percent)
{
  if (percent > 100u) percent = 100u;
  if (s_amp_pct == percent) return;
  s_amp_pct = percent;
  Scan_HW_Stop();
  Scan_Rebuild();
  if (s_running) Scan_HW_Start();
}

void Scan_SetIntervalUs(uint32_t us)
{
  if (us < SCAN_MIN_INTERVAL_US) us = SCAN_MIN_INTERVAL_US;
  if (s_interval_us == us) return;
  s_interval_us = us;
  Scan_HW_Stop();
  Scan_TimerSetInterval(us);
  if (s_running) Scan_HW_Start();
}

uint8_t Scan_IsIdle(void)
{
  return (uint8_t)(s_running == 0u);
}

/* ---- 硬件中断回调（TIM6 点步进由生成代码 HAL_TIM_IRQHandler 转入 -> main.c 调本函数；SPI DMA 帧完成） ---- */
void Scan_TimerTick(void)
{
  if (!s_running) return;
  SCAN_PIN_SYNC_LOW();
  s_dma_stage = 0u;
  HAL_SPI_Transmit_DMA(&hspi3, &s_frame[(uint32_t)s_pt_index * SCAN_FRAME_BYTES], 3u);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != &hspi3) return;
  if (s_dma_stage == 0u)
  {
    /* X 帧完成：SYNC 高（锁存 X），随即拉低并触发 Y 帧 */
    SCAN_PIN_SYNC_HIGH();
    s_dma_stage = 1u;
    SCAN_PIN_SYNC_LOW();
    HAL_SPI_Transmit_DMA(&hspi3, &s_frame[(uint32_t)s_pt_index * SCAN_FRAME_BYTES + 3u], 3u);
  }
  else
  {
    /* Y 帧完成：SYNC 高（锁存 Y），推进点序号 */
    SCAN_PIN_SYNC_HIGH();
    s_pt_index++;
    if (s_pt_index >= s_pt_count) s_pt_index = 0u;
  }
}