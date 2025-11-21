/**
 ******************************************************************************
 * @file    bsp_spi.h
 * @brief   BSP层SPI硬件驱动接口（含DMA总线管理器）
 * @note    提供W25Qxx和SD卡共享SPI1 DMA通道的管理机制
 * @version V2.0.0
 * @date    2025-01-XX
 ******************************************************************************
 */

#ifndef BSP_SPI_H
#define BSP_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "bsp_common.h"
#include "gpio.h"
/* Exported constants --------------------------------------------------------*/

#define BSP_SPI_TIMEOUT 100 ///< SPI超时时间（毫秒）

/* Exported types ------------------------------------------------------------*/

/**
 * @brief SPI DMA设备枚举
 */
typedef enum {
    BSP_SPI_DMA_DEVICE_NONE    = 0, ///< 空闲状态
    BSP_SPI_DMA_DEVICE_W25QXX  = 1, ///< W25Qxx Flash
    BSP_SPI_DMA_DEVICE_SD_CARD = 2  ///< SD卡
} bsp_spi_dma_device_t;

/**
 * @brief SPI DMA操作状态码
 */
typedef enum {
    BSP_SPI_DMA_OK      = 0, ///< 操作成功
    BSP_SPI_DMA_BUSY    = 1, ///< DMA忙碌（被其他设备占用）
    BSP_SPI_DMA_TIMEOUT = 2, ///< 超时
    BSP_SPI_DMA_ERROR   = 3  ///< 错误
} bsp_spi_dma_status_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 尝试锁定DMA资源
 * @param device 请求锁定的设备
 * @return bool true=成功锁定, false=被占用
 * @note 使用原子操作确保线程安全
 */
bool                 bsp_spi_dma_lock(bsp_spi_dma_device_t device);

/**
 * @brief 释放DMA资源
 * @param device 释放锁定的设备
 */
void                 bsp_spi_dma_unlock(bsp_spi_dma_device_t device);

/**
 * @brief SPI DMA全双工传输（阻塞式，带超时）
 * @param tx_buf 发送缓冲区（NULL则发送0xFF）
 * @param rx_buf 接收缓冲区（NULL则丢弃接收数据）
 * @param len 传输长度
 * @param timeout_ms 超时时间（毫秒）
 * @return bsp_spi_dma_status_t 操作状态
 * @note 调用前必须先成功调用bsp_spi_dma_lock()
 * @note 使用LL库直接操作DMA寄存器，提升性能
 */
bsp_spi_dma_status_t bsp_spi_dma_transmit_receive(const uint8_t* tx_buf,
                                                  uint8_t* rx_buf, uint16_t len,
                                                  uint32_t timeout_ms);

/**
 * @brief 获取当前占用DMA的设备
 * @return bsp_spi_dma_device_t 当前设备
 */
bsp_spi_dma_device_t bsp_spi_dma_get_current_device(void);

/* Exported callback functions -----------------------------------------------*/
/* @note 以下函数由stm32g4xx_it.c中的DMA中断处理器调用 */

/**
 * @brief DMA RX完成回调（在DMA1_Channel1_IRQHandler中调用）
 */
void                 bsp_spi_dma_rx_complete_callback(void);

/**
 * @brief DMA TX完成回调（在DMA1_Channel2_IRQHandler中调用）
 */
void                 bsp_spi_dma_tx_complete_callback(void);

/**
 * @brief DMA错误回调（在DMA错误中断中调用）
 */
void                 bsp_spi_dma_error_callback(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_SPI_H
