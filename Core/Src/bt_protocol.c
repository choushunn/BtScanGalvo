/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bt_protocol.c
  * @brief   蓝牙帧协议实现：状态机组帧 + CRC8
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "bt_protocol.h"

/* 接收状态机状态 */
#define PS_WAIT_SOF  0u
#define PS_WAIT_LEN  1u
#define PS_WAIT_BODY 2u   /* 正在收 CMD+PAYLOAD（payload_len 已定） */
#define PS_WAIT_CRC  3u

static uint8_t ps_state = PS_WAIT_SOF;
static uint8_t ps_len;
static uint8_t ps_idx;
static BT_Frame ps_frame;   /* 组装中的帧 */
static uint8_t ps_crc;      /* 计算中的校验 */

static uint8_t crc_update(uint8_t crc, uint8_t b)
{
  uint8_t bit;
  crc ^= b;
  for (bit = 0u; bit < 8u; bit++)
  {
    if (crc & 0x80u) crc = (uint8_t)((crc << 1) ^ 0x07u);
    else             crc = (uint8_t)(crc << 1);
  }
  return crc;
}

uint8_t BT_CRC8(const uint8_t *data, uint8_t len)
{
  uint8_t crc = 0u;
  uint8_t i;
  for (i = 0u; i < len; i++) crc = crc_update(crc, data[i]);
  return crc;
}

uint8_t BT_ParseByte(uint8_t byte, BT_Frame *frame)
{
  uint8_t done = 0u;

  switch (ps_state)
  {
    case PS_WAIT_SOF:
      if (byte == BT_SOF) ps_state = PS_WAIT_LEN;
      break;

    case PS_WAIT_LEN:
      ps_len  = byte;
      ps_idx  = 0u;
      ps_crc  = 0u;
      if (ps_len > BT_MAX_PAYLOAD) ps_state = PS_WAIT_SOF; /* 非法长度，重新同步 */
      else                         ps_state = PS_WAIT_BODY;
      break;

    case PS_WAIT_BODY:
      ps_crc = crc_update(ps_crc, byte);   /* CMD 与 PAYLOAD 全部纳入校验 */
      if (ps_idx == 0u)
      {
        ps_frame.cmd = byte;
        ps_frame.payload_len = 0u;
      }
      else
      {
        ps_frame.payload[ps_idx - 1u] = byte;
        ps_frame.payload_len = (uint8_t)ps_idx;
      }
      ps_idx++;
      if (ps_idx == (uint8_t)(ps_len + 1u)) ps_state = PS_WAIT_CRC; /* CMD+payload 收完 */
      break;

    case PS_WAIT_CRC:
      if (byte == ps_crc)
      {
        *frame = ps_frame;
        done = 1u;
      }
      ps_state = PS_WAIT_SOF;              /* 无论对错都回到找 SOF */
      break;

    default:
      ps_state = PS_WAIT_SOF;
      break;
  }
  return done;
}

uint8_t BT_BuildFrame(uint8_t cmd, const uint8_t *payload, uint8_t payload_len, uint8_t *out)
{
  uint8_t n = 0u;
  uint8_t body[1u + BT_MAX_PAYLOAD];
  uint8_t i;

  if (payload_len > BT_MAX_PAYLOAD) payload_len = BT_MAX_PAYLOAD;

  body[0] = cmd;
  for (i = 0u; i < payload_len; i++) body[1u + i] = payload[i];

  out[n++] = BT_SOF;
  out[n++] = payload_len;
  out[n++] = cmd;
  for (i = 0u; i < payload_len; i++) out[n++] = payload[i];
  out[n++] = BT_CRC8(body, (uint8_t)(1u + payload_len));
  return n;
}