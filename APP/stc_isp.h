/**
 ******************************************************************************
 * @file    stc_isp.h
 * @brief   STC8 ISP handshake helpers for STM32 offline programmer demo
 ******************************************************************************
 * @attention
 * - Based on stcgal protocol behavior, trimmed to handshake/status only.
 ******************************************************************************
 */

#ifndef __STC_ISP_H__
#define __STC_ISP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* STC frame constants */
#define STC_PACKET_START_0           0x46
#define STC_PACKET_START_1           0xB9
#define STC_PACKET_END               0x16
#define STC_PACKET_MCU               0x68
#define STC_PACKET_HOST              0x6A

/* STC8G1K08A magic */
#define STC8G1K08A_MAGIC             0xF794

/* Baudrates */
#define STC_HANDSHAKE_BAUD           2400U
#define STC_DEFAULT_BAUD             115200U

/* Sync pulse byte */
#define STC_SYNC_CHAR                0x7F

/* Status packet payload size limit */
#define STC_STATUS_PACKET_MAX_LEN    100

/* Power switch pin (cold start) */
#define STC_POWER_CTRL_PIN           LL_GPIO_PIN_11
#define STC_POWER_CTRL_PORT          GPIOA

typedef enum {
    STC_ISP_OK = 0,
    STC_ISP_ERROR_TIMEOUT,
    STC_ISP_ERROR_FRAMING,
    STC_ISP_ERROR_PROTOCOL,
    STC_ISP_ERROR_MAGIC_MISMATCH
} STC_ISP_StatusTypeDef;

typedef struct {
    uint8_t  data[STC_STATUS_PACKET_MAX_LEN];
    uint16_t length;
    uint16_t mcu_magic;
    uint32_t mcu_clock_hz;
    char     bsl_version[16];
} STC_StatusPacketTypeDef;

STC_ISP_StatusTypeDef STC_ISP_Init(USART_TypeDef *uart_instance);
void                  STC_ISP_ColdStart(void);
STC_ISP_StatusTypeDef STC_ISP_ReconfigureUart(USART_TypeDef *uart_instance, uint32_t baudrate, uint32_t parity);
STC_ISP_StatusTypeDef STC_ISP_SendSyncPulse(USART_TypeDef *uart_instance, uint32_t timeout_ms);
STC_ISP_StatusTypeDef STC_ISP_ReadStatusPacket(USART_TypeDef *uart_instance,
                                                STC_StatusPacketTypeDef *status_packet,
                                                uint32_t timeout_ms);
STC_ISP_StatusTypeDef STC_ISP_ParseStatusPacket(STC_StatusPacketTypeDef *status_packet);
STC_ISP_StatusTypeDef STC_ISP_HandshakeTest(USART_TypeDef *uart_instance);
void                  STC_ISP_PrintStatusPacket(STC_StatusPacketTypeDef *status_packet);

#ifdef __cplusplus
}
#endif

#endif /* __STC_ISP_H__ */
