/**
 ******************************************************************************
 * @file    bsp_spi.c
 * @brief   BSP层SPI硬件驱动实现（DMA总线管理器）
 * @note    从原spi_bus_manager.c迁移，保持功能一致性
 * @version V2.0.0
 * @date    2025-01-XX
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "bsp_spi.h"
#include "stm32f1xx_ll_spi.h"
#include "stm32f1xx_ll_dma.h"

/* Private types -------------------------------------------------------------*/

/**
 * @brief DMA管理器状态结构体
 */
typedef struct {
    volatile bsp_spi_dma_device_t current_device;  ///< 当前占用设备
    volatile bool                 tx_complete;     ///< TX完成标志
    volatile bool                 rx_complete;     ///< RX完成标志
    volatile uint32_t             error_code;      ///< 错误码
} bsp_spi_dma_manager_t;

/* Private variables ---------------------------------------------------------*/

/**
 * @brief DMA管理器全局状态
 */
static bsp_spi_dma_manager_t g_dma_mgr = {
    .current_device = BSP_SPI_DMA_DEVICE_NONE,
    .tx_complete    = false,
    .rx_complete    = false,
    .error_code     = 0
};

/**
 * @brief 静态dummy缓冲区（避免栈溢出）
 */
static uint8_t g_dummy_tx = 0xFF;  ///< 发送dummy字节
static uint8_t g_dummy_rx = 0x00;  ///< 接收dummy字节

/* Public functions ----------------------------------------------------------*/

/**
 * @brief 尝试锁定DMA资源
 */
bool bsp_spi_dma_lock(bsp_spi_dma_device_t device)
{
    if (device == BSP_SPI_DMA_DEVICE_NONE) {
        return false;  // 无效设备
    }

    // 原子性检查和设置
    __disable_irq();
    if (g_dma_mgr.current_device == BSP_SPI_DMA_DEVICE_NONE) {
        g_dma_mgr.current_device = device;
        __enable_irq();
        return true;
    }
    __enable_irq();

    return false;  // 已被占用
}

/**
 * @brief 释放DMA资源
 */
void bsp_spi_dma_unlock(bsp_spi_dma_device_t device)
{
    __disable_irq();
    if (g_dma_mgr.current_device == device) {
        g_dma_mgr.current_device = BSP_SPI_DMA_DEVICE_NONE;
    }
    __enable_irq();
}

/**
 * @brief SPI DMA全双工传输
 */
bsp_spi_dma_status_t bsp_spi_dma_transmit_receive(
    const uint8_t* tx_buf,
    uint8_t* rx_buf,
    uint16_t len,
    uint32_t timeout_ms
)
{
    if (len == 0) {
        return BSP_SPI_DMA_OK;
    }

    // 检查是否已锁定
    if (g_dma_mgr.current_device == BSP_SPI_DMA_DEVICE_NONE) {
        return BSP_SPI_DMA_ERROR;  // 未锁定DMA
    }

    // 处理NULL缓冲区
    uint32_t tx_addr = tx_buf ? (uint32_t)tx_buf : (uint32_t)&g_dummy_tx;
    uint32_t rx_addr = rx_buf ? (uint32_t)rx_buf : (uint32_t)&g_dummy_rx;

    // === 1. 禁用DMA通道 ===
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);  // RX
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);  // TX

    // === 2. 配置RX DMA (Channel 2: SPI1_RX) ===
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_2, rx_addr);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_2, (uint32_t)&SPI1->DR);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_2, len);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_2,
                            rx_buf ? LL_DMA_MEMORY_INCREMENT : LL_DMA_MEMORY_NOINCREMENT);

    // === 3. 配置TX DMA (Channel 3: SPI1_TX) ===
    LL_DMA_SetMemoryAddress(DMA1, LL_DMA_CHANNEL_3, tx_addr);
    LL_DMA_SetPeriphAddress(DMA1, LL_DMA_CHANNEL_3, (uint32_t)&SPI1->DR);
    LL_DMA_SetDataLength(DMA1, LL_DMA_CHANNEL_3, len);
    LL_DMA_SetMemoryIncMode(DMA1, LL_DMA_CHANNEL_3,
                            tx_buf ? LL_DMA_MEMORY_INCREMENT : LL_DMA_MEMORY_NOINCREMENT);

    // === 4. 清除DMA标志 ===
    LL_DMA_ClearFlag_TC2(DMA1);
    LL_DMA_ClearFlag_TE2(DMA1);
    LL_DMA_ClearFlag_TC3(DMA1);
    LL_DMA_ClearFlag_TE3(DMA1);

    // === 5. 重置完成标志 ===
    g_dma_mgr.tx_complete = false;
    g_dma_mgr.rx_complete = false;
    g_dma_mgr.error_code  = 0;

    // === 6. 使能DMA中断 ===
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_2);  // RX传输完成中断
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_2);  // RX传输错误中断
    LL_DMA_EnableIT_TC(DMA1, LL_DMA_CHANNEL_3);  // TX传输完成中断
    LL_DMA_EnableIT_TE(DMA1, LL_DMA_CHANNEL_3);  // TX传输错误中断

    // === 7. 使能SPI DMA请求 ===
    LL_SPI_EnableDMAReq_RX(SPI1);
    LL_SPI_EnableDMAReq_TX(SPI1);

    // === 8. 启动DMA传输（先RX后TX，确保不丢数据） ===
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_2);  // RX
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_3);  // TX

    // === 9. 等待传输完成（带超时） ===
    uint32_t start_tick = HAL_GetTick();
    while (!g_dma_mgr.tx_complete || !g_dma_mgr.rx_complete) {
        if ((HAL_GetTick() - start_tick) > timeout_ms) {
            // 超时处理
            LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
            LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
            LL_SPI_DisableDMAReq_RX(SPI1);
            LL_SPI_DisableDMAReq_TX(SPI1);
            return BSP_SPI_DMA_TIMEOUT;
        }

        // 检查错误
        if (g_dma_mgr.error_code) {
            LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_2);
            LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_3);
            LL_SPI_DisableDMAReq_RX(SPI1);
            LL_SPI_DisableDMAReq_TX(SPI1);
            return BSP_SPI_DMA_ERROR;
        }
    }

    // === 10. 等待SPI忙标志清除 ===
    start_tick = HAL_GetTick();
    while (LL_SPI_IsActiveFlag_BSY(SPI1)) {
        if ((HAL_GetTick() - start_tick) > 100) {
            break;  // 避免死锁
        }
    }

    // === 11. 禁用DMA请求 ===
    LL_SPI_DisableDMAReq_RX(SPI1);
    LL_SPI_DisableDMAReq_TX(SPI1);

    return BSP_SPI_DMA_OK;
}

/**
 * @brief 获取当前占用DMA的设备
 */
bsp_spi_dma_device_t bsp_spi_dma_get_current_device(void)
{
    return g_dma_mgr.current_device;
}

/* Interrupt callbacks -------------------------------------------------------*/

/**
 * @brief DMA RX完成回调
 */
void bsp_spi_dma_rx_complete_callback(void)
{
    g_dma_mgr.rx_complete = true;
}

/**
 * @brief DMA TX完成回调
 */
void bsp_spi_dma_tx_complete_callback(void)
{
    g_dma_mgr.tx_complete = true;
}

/**
 * @brief DMA错误回调
 */
void bsp_spi_dma_error_callback(void)
{
    g_dma_mgr.error_code  = 1;
    g_dma_mgr.rx_complete = true;  // 设置标志以唤醒等待
    g_dma_mgr.tx_complete = true;
}

