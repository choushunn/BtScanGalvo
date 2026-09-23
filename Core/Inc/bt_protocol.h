/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    bt_protocol.h
  * @brief   蓝牙自定义二进制帧协议：组帧/解析/CRC，命令常量与可扩展命令表
  ******************************************************************************
  */
/* USER CODE END Header */
#ifndef __BT_PROTOCOL_H
#define __BT_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 帧格式: [SOF 0xAA][LEN=payload长度][CMD][PAYLOAD(LEN)][CRC8]
   CRC8 覆盖 [CMD..PAYLOAD] */
#define BT_SOF              0xAAu
#define BT_MAX_PAYLOAD      8u

/* 命令定义（0x80 以上保留给扩展) */
#define BT_CMD_SWITCH_SHAPE    0x01u   /* payload[0]=形状(0方/1长方/2圆) */
#define BT_CMD_SET_AMPLITUDE   0x02u   /* payload[0]=0..100 幅度% */
#define BT_CMD_SET_SCAN_STATE  0x03u   /* payload[0]=0停/1启/2暂停 */
#define BT_CMD_SET_INTERVAL    0x04u   /* payload[0..1]=点间隔us(LittleEndian) */
#define BT_CMD_ACK             0xF0u   /* 回执: payload[0]=错误码 */

/* ACK 错误码 */
#define BT_ERR_NONE   0u
#define BT_ERR_CMD    1u   /* 未知命令 */

typedef struct {
  uint8_t cmd;
  uint8_t payload_len;
  uint8_t payload[BT_MAX_PAYLOAD];
} BT_Frame;

/* 逐字节喂入串口流，返回 1 表示得到一帧完整且校验通过的命令帧（填充 frame）*/
uint8_t BT_ParseByte(uint8_t byte, BT_Frame *frame);

/* CRC8（多项式 0x07） */
uint8_t BT_CRC8(const uint8_t *data, uint8_t len);

/* 序列化一帧到 out，返回总字节数（<=2+8+1=11） */
uint8_t BT_BuildFrame(uint8_t cmd, const uint8_t *payload, uint8_t payload_len, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __BT_PROTOCOL_H */
