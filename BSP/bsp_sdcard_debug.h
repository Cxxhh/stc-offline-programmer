/**
 ******************************************************************************
 * @file    bsp_sdcard_debug.h
 * @brief   SD卡调试辅助函数
 * @note    用于诊断SD卡识别问题
 ******************************************************************************
 */

#ifndef BSP_SDCARD_DEBUG_H
#define BSP_SDCARD_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_sdcard.h"
#include <stdio.h>

/**
 * @brief 诊断SD卡初始化问题
 * @param debug_buf 调试信息缓冲区（建议512字节）
 * @param buf_size 缓冲区大小
 * @return 返回诊断信息字符串
 */
static inline const char* bsp_sdcard_diagnose(char* debug_buf, size_t buf_size)
{
    if (debug_buf == NULL || buf_size < 256) {
        return "Buffer too small";
    }
    
    debug_buf[0] = '\0';
    char temp[128];
    
    // 1. 检查CS引脚
    GPIO_PinState cs_state = HAL_GPIO_ReadPin(SD_CS_GPIO_Port, SD_CS_Pin);
    snprintf(temp, sizeof(temp), "CS Pin: %s\n", cs_state == GPIO_PIN_SET ? "HIGH(OK)" : "LOW(BAD)");
    strncat(debug_buf, temp, buf_size - strlen(debug_buf) - 1);
    
    // 2. 测试SPI通信
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    
    LL_SPI_TransmitData8(SPI1, 0xFF);
    uint32_t timeout = 10000;
    while (!LL_SPI_IsActiveFlag_RXNE(SPI1) && timeout--);
    uint8_t rx = LL_SPI_ReceiveData8(SPI1);
    
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    
    snprintf(temp, sizeof(temp), "SPI Test: TX=0xFF, RX=0x%02X %s\n", 
             rx, (rx == 0xFF) ? "(OK)" : "(Check MISO)");
    strncat(debug_buf, temp, buf_size - strlen(debug_buf) - 1);
    
    // 3. 尝试CMD0
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
    HAL_Delay(1);
    
    // 发送CMD0
    LL_SPI_TransmitData8(SPI1, 0x40); // CMD0
    while (!LL_SPI_IsActiveFlag_RXNE(SPI1));
    LL_SPI_ReceiveData8(SPI1);
    
    for (int i = 0; i < 5; i++) {
        uint8_t data = (i == 4) ? 0x95 : 0x00;
        LL_SPI_TransmitData8(SPI1, data);
        while (!LL_SPI_IsActiveFlag_RXNE(SPI1));
        LL_SPI_ReceiveData8(SPI1);
    }
    
    // 读取响应
    uint8_t r1 = 0xFF;
    for (int i = 0; i < 10; i++) {
        LL_SPI_TransmitData8(SPI1, 0xFF);
        while (!LL_SPI_IsActiveFlag_RXNE(SPI1));
        r1 = LL_SPI_ReceiveData8(SPI1);
        if (!(r1 & 0x80)) break;
    }
    
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    
    snprintf(temp, sizeof(temp), "CMD0 Response: 0x%02X %s\n", 
             r1, (r1 == 0x01) ? "(OK-IDLE)" : (r1 == 0xFF ? "(NO CARD)" : "(ERROR)"));
    strncat(debug_buf, temp, buf_size - strlen(debug_buf) - 1);
    
    // 4. 建议
    strncat(debug_buf, "\nTroubleshooting:\n", buf_size - strlen(debug_buf) - 1);
    
    if (rx == 0xFF && r1 == 0xFF) {
        strncat(debug_buf, "- SD card not inserted or bad contact\n", buf_size - strlen(debug_buf) - 1);
        strncat(debug_buf, "- Check MISO connection\n", buf_size - strlen(debug_buf) - 1);
    } else if (rx == 0x00) {
        strncat(debug_buf, "- MISO stuck LOW, check wiring\n", buf_size - strlen(debug_buf) - 1);
    } else if (r1 != 0x01 && r1 != 0xFF) {
        strncat(debug_buf, "- Card detected but init failed\n", buf_size - strlen(debug_buf) - 1);
        strncat(debug_buf, "- Try slower SPI speed (DIV256)\n", buf_size - strlen(debug_buf) - 1);
    } else if (r1 == 0x01) {
        strncat(debug_buf, "- CMD0 OK! Check CMD8/ACMD41\n", buf_size - strlen(debug_buf) - 1);
    }
    
    return debug_buf;
}

#ifdef __cplusplus
}
#endif

#endif // BSP_SDCARD_DEBUG_H

