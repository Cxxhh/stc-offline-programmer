/**
 ******************************************************************************
 * @file    bsp_sdcard.c
 * @brief   BSP层SD卡硬件驱动实现（SPI模式）
 * @note    从原SD.c迁移并简化，保留核心功能
 * @version V2.0.0
 * @date    2025-01-XX
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "bsp_sdcard.h"
#include "bsp_spi.h"
#include "stm32g4xx_ll_spi.h"
#include "stm32g4xx_hal.h"
#include "gpio.h"
#include <stdio.h>
/* Private defines -----------------------------------------------------------*/

// SD卡命令定义
#define SD_CMD0 0   ///< 卡复位
#define SD_CMD1 1   ///< MMC初始化
#define SD_CMD8 8   ///< SEND_IF_COND
#define SD_CMD9 9   ///< 读CSD数据
#define SD_CMD12 12 ///< 停止数据传输
#define SD_CMD16 16 ///< 设置扇区大小
#define SD_CMD17 17 ///< 读单个扇区
#define SD_CMD18 18 ///< 读多个扇区
#define SD_CMD24 24 ///< 写单个扇区
#define SD_CMD25 25 ///< 写多个扇区
#define SD_CMD41 41 ///< ACMD41
#define SD_CMD55 55 ///< APP_CMD
#define SD_CMD58 58 ///< 读OCR信息
#define SD_CMD13 13 ///< 读卡状态

// SD卡响应标志
#define SD_RESPONSE_NO_ERROR 0x00
#define SD_IN_IDLE_STATE 0x01
#define SD_DATA_START_TOKEN 0xFE
#define SD_DATA_ACCEPTED 0x05

/* Private variables ---------------------------------------------------------*/

static bsp_sdcard_type_t   g_sd_type         = BSP_SDCARD_TYPE_UNKNOWN;
static bsp_sdcard_status_t g_sd_status       = BSP_SDCARD_STATUS_NO_CARD;
static bool                g_bsp_initialized = false; // BSP层初始化标志

/* Private function prototypes -----------------------------------------------*/

// SPI传输函数
static inline uint8_t      _bsp_sdcard_spi_rw(uint8_t data);

// SD卡命令函数
static uint8_t _bsp_sdcard_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc);
static uint8_t _bsp_sdcard_wait_ready(void);
static uint8_t _bsp_sdcard_receive_data(uint8_t* buf, uint16_t len);
static uint8_t _bsp_sdcard_send_data(const uint8_t* buf, uint8_t token);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief 选中SD卡（CS引脚互斥控制）
 */
void           bsp_sdcard_select(void)
{
    // 如果有其他SPI设备（如Flash），需要在这里取消选择它们
    // 例如: FLASH_CS_GPIO_Port->BSRR = FLASH_CS_Pin;

    // 拉低CS，选中SD卡
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
}

/**
 * @brief 取消选择SD卡
 */
void bsp_sdcard_deselect(void)
{
    // 拉高CS，取消选择SD卡
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief SPI读写单字节
 */
static inline uint8_t _bsp_sdcard_spi_rw(uint8_t data)
{
    LL_SPI_TransmitData8(SPI1, data);
    while (!LL_SPI_IsActiveFlag_RXNE(SPI1))
        ;
    return LL_SPI_ReceiveData8(SPI1);
}

/**
 * @brief 发送SD卡命令
 */
static uint8_t _bsp_sdcard_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t  r1;
    uint16_t retry = 0;

    bsp_sdcard_deselect();
    bsp_sdcard_select();

    // 等待卡准备好
    retry = 100;
    while (_bsp_sdcard_spi_rw(0xFF) != 0xFF && retry--)
        ;

    // 发送命令
    _bsp_sdcard_spi_rw(cmd | 0x40);
    _bsp_sdcard_spi_rw(arg >> 24);
    _bsp_sdcard_spi_rw(arg >> 16);
    _bsp_sdcard_spi_rw(arg >> 8);
    _bsp_sdcard_spi_rw(arg);
    _bsp_sdcard_spi_rw(crc);

    if (cmd == SD_CMD12) {
        _bsp_sdcard_spi_rw(0xFF); // CMD12需要额外的字节
    }

    // 等待响应
    retry = 10000;
    do {
        r1 = _bsp_sdcard_spi_rw(0xFF);
        if (retry-- == 0) {
            return 0xFF; // 超时
        }
    } while (r1 & 0x80);

    return r1;
}

// /**
//  * @brief 等待SD卡准备就绪
//  */
// static uint8_t _bsp_sdcard_wait_ready(void)
// {
//     uint32_t timeout = HAL_GetTick() + 500;

//     do {
//         if (_bsp_sdcard_spi_rw(0xFF) == 0xFF) { return BSP_SDCARD_OK; }
//     } while (HAL_GetTick() < timeout);

//     return BSP_SDCARD_TIMEOUT;
// }

/**
 * @brief 接收SD卡数据块
 */
static uint8_t _bsp_sdcard_receive_data(uint8_t* buf, uint16_t len)
{
    uint8_t  token;
    uint32_t timeout = HAL_GetTick() + 200;

    // 等待数据起始令牌
    do {
        token = _bsp_sdcard_spi_rw(0xFF);
        if (HAL_GetTick() >= timeout) { return BSP_SDCARD_TIMEOUT; }
    } while (token != SD_DATA_START_TOKEN);

    // 读取数据
    for (uint16_t i = 0; i < len; i++) {
        buf[i] = _bsp_sdcard_spi_rw(0xFF);
    }

    // 读取CRC（忽略）
    _bsp_sdcard_spi_rw(0xFF);
    _bsp_sdcard_spi_rw(0xFF);

    return BSP_SDCARD_OK;
}

/**
 * @brief 发送SD卡数据块（移植自主工程SD_SendBlock + SD_WriteDisk_DMA）
 * @note 完全匹配主工程的实现逻辑，包括busy等待
 */
static uint8_t _bsp_sdcard_send_data(const uint8_t* buf, uint8_t token)
{
    uint16_t t;
    uint8_t  resp;

    // 等待卡准备好
    uint32_t wait_count = 0;
    do {
        resp = _bsp_sdcard_spi_rw(0xFF);
        wait_count++;
        if (wait_count > 100000) {
            // bsp_uart_send_string("ERROR: SD card not ready\r\n");
            return 2;
        }
    } while (resp != 0xFF);

    // 发送数据起始令牌
    _bsp_sdcard_spi_rw(token);

    if (token != 0xFD) { // 如果不是停止传输令牌
        // 发送512字节数据
        for (t = 0; t < 512; t++) {
            _bsp_sdcard_spi_rw(buf[t]);
        }

        // 发送CRC（忽略）
        _bsp_sdcard_spi_rw(0xFF);
        _bsp_sdcard_spi_rw(0xFF);

        // 接收响应
        t = _bsp_sdcard_spi_rw(0xFF);
        if ((t & 0x1F) != 0x05) {
            // bsp_uart_send_string("ERROR: SD card write response error\r\n");
            return 2;
        }

        // ✅ 关键：等待SD卡内部写入完成（busy信号清除）
        // SD卡写入时会拉低MISO线（返回0x00），完成后释放（返回0xFF）
        wait_count = 0;
        while (_bsp_sdcard_spi_rw(0xFF) == 0x00) {
            wait_count++;
            if (wait_count > 1000000) {
                // bsp_uart_send_string("ERROR: SD card write timeout\r\n");
                return 2;
            }
        }
    }

    return 0; // 写入成功
}

/* Public functions ----------------------------------------------------------*/

/**
 * @brief 初始化SD卡
 * @note 支持重复调用（幂等操作）- 如已初始化，直接返回之前的状态
 */
bsp_sdcard_result_t bsp_sdcard_init(void)
{
    uint8_t  r1;
    uint16_t retry;

    // 幂等性检查：如果已成功初始化，直接返回成功
    if (g_bsp_initialized && g_sd_type != BSP_SDCARD_TYPE_UNKNOWN) {
        return BSP_SDCARD_OK;
    }

    // 重置状态（允许重新初始化）
    g_sd_type = BSP_SDCARD_TYPE_UNKNOWN;
    g_sd_status = BSP_SDCARD_STATUS_NO_CARD;
    g_bsp_initialized = false;

    // CS引脚初始化（拉高，取消选择）
    bsp_sdcard_deselect();
    
    // 添加延时，让SD卡上电稳定
    HAL_Delay(10);

    // 发送至少74个时钟脉冲（让SD卡进入SPI模式）
    for (uint8_t i = 0; i < 20; i++) {  // 增加到20次，更可靠
        _bsp_sdcard_spi_rw(0xFF);
    }

    // CMD0：复位SD卡（与APP保持一致）
    retry = 200;
    do {
        r1 = _bsp_sdcard_send_cmd(SD_CMD0, 0, 0x95);
        HAL_Delay(1);  // 每次尝试之间加小延时
    } while ((r1 != SD_IN_IDLE_STATE) && retry--);

    if (retry == 0) {
        bsp_sdcard_deselect();
        g_sd_status = BSP_SDCARD_STATUS_NO_CARD;
        return BSP_SDCARD_ERROR;
    }

    // CMD8：检查SD卡版本
    r1 = _bsp_sdcard_send_cmd(SD_CMD8, 0x1AA, 0x87);
    if (r1 == SD_IN_IDLE_STATE) {
        // SD V2.0+
        uint8_t ocr[4];
        for (uint8_t i = 0; i < 4; i++) {
            ocr[i] = _bsp_sdcard_spi_rw(0xFF);
        }

        // 初始化SD V2.0卡（与APP保持一致）
        retry = 10000;
        do {
            _bsp_sdcard_send_cmd(SD_CMD55, 0, 0xFF);
            r1 = _bsp_sdcard_send_cmd(SD_CMD41, 0x40000000, 0xFF);
        } while (r1 && retry--);

        if (retry == 0) {
            bsp_sdcard_deselect();
            g_sd_status = BSP_SDCARD_STATUS_NO_CARD;
            return BSP_SDCARD_ERROR;
        }

        // 读取OCR
        r1 = _bsp_sdcard_send_cmd(SD_CMD58, 0, 0xFF);
        if (r1 == 0) {
            for (uint8_t i = 0; i < 4; i++) {
                ocr[i] = _bsp_sdcard_spi_rw(0xFF);
            }

            // 检查CCS位（Card Capacity Status）
            if (ocr[0] & 0x40) {
                g_sd_type = BSP_SDCARD_TYPE_V2HC; // SDHC
            }
            else {
                g_sd_type = BSP_SDCARD_TYPE_V2; // SD V2.0标准卡
            }
        }
    }
    else {
        // SD V1.x 或 MMC
        _bsp_sdcard_send_cmd(SD_CMD55, 0, 0xFF);
        r1    = _bsp_sdcard_send_cmd(SD_CMD41, 0, 0xFF);

        retry = 10000;  // 与APP保持一致
        if (r1 <= 1) {
            // SD V1.x
            g_sd_type = BSP_SDCARD_TYPE_V1;
            do {
                _bsp_sdcard_send_cmd(SD_CMD55, 0, 0xFF);
                r1 = _bsp_sdcard_send_cmd(SD_CMD41, 0, 0xFF);
            } while (r1 && retry--);
        }
        else {
            // MMC卡
            g_sd_type = BSP_SDCARD_TYPE_MMC;
            do {
                r1 = _bsp_sdcard_send_cmd(SD_CMD1, 0, 0xFF);
            } while (r1 && retry--);
        }

        if (retry == 0) {
            bsp_sdcard_deselect();
            g_sd_status = BSP_SDCARD_STATUS_NO_CARD;
            return BSP_SDCARD_ERROR;
        }
    }

    // 设置块大小为512字节
    if (g_sd_type != BSP_SDCARD_TYPE_V2HC) {
        r1 = _bsp_sdcard_send_cmd(SD_CMD16, 512, 0xFF);
        if (r1 != 0) {
            bsp_sdcard_deselect();
            g_sd_status = BSP_SDCARD_STATUS_NO_CARD;
            return BSP_SDCARD_ERROR;
        }
    }

    bsp_sdcard_deselect();
    g_sd_status       = BSP_SDCARD_STATUS_CARD_READY;
    g_bsp_initialized = true; // 标记已初始化

    return BSP_SDCARD_OK;
}

/**
 * @brief 检测SD卡并读取信息
 */
bsp_sdcard_result_t bsp_sdcard_detect(bsp_sdcard_info_t* info)
{
    if (info == NULL) { return BSP_SDCARD_ERROR; }

    info->type         = g_sd_type;
    info->block_size   = BSP_SDCARD_BLOCK_SIZE;
    info->detected     = (g_sd_type != BSP_SDCARD_TYPE_UNKNOWN);

    // 读取扇区数（简化实现，实际需要读取CSD寄存器）
    info->sector_count = 0;
    info->capacity_mb  = 0;

    return BSP_SDCARD_OK;
}

/**
 * @brief 检查SD卡是否插入
 */
bool bsp_sdcard_is_inserted(void)
{
    return (g_sd_type != BSP_SDCARD_TYPE_UNKNOWN);
}

bool bsp_sdcard_detect_fast(void)
{
    uint8_t  r1;
    uint16_t retry = 200;

    bsp_sdcard_deselect();
    for (uint8_t i = 0; i < 10; i++) {
        _bsp_sdcard_spi_rw(0xFF);
    }

    bsp_sdcard_select();

    uint16_t wait_ready = 100;
    while (_bsp_sdcard_spi_rw(0xFF) != 0xFF && wait_ready--) {
        /* wait for line idle */
    }

    _bsp_sdcard_spi_rw(SD_CMD0 | 0x40);
    _bsp_sdcard_spi_rw(0);
    _bsp_sdcard_spi_rw(0);
    _bsp_sdcard_spi_rw(0);
    _bsp_sdcard_spi_rw(0);
    _bsp_sdcard_spi_rw(0x95);

    do {
        r1 = _bsp_sdcard_spi_rw(0xFF);
        if (retry-- == 0) {
            bsp_sdcard_deselect();
            return false;
        }
    } while (r1 & 0x80);

    bsp_sdcard_deselect();

    return (r1 == SD_IN_IDLE_STATE);
}

bool bsp_sdcard_check_alive(void)
{
    if (g_sd_type == BSP_SDCARD_TYPE_UNKNOWN || !g_bsp_initialized) {
        return false;
    }

    uint8_t r1 = _bsp_sdcard_send_cmd(SD_CMD13, 0, 0x01);
    _bsp_sdcard_spi_rw(0xFF); // R2 second byte
    bsp_sdcard_deselect();

    if (r1 == 0x00 || r1 == 0x01) { return true; }

    g_sd_type         = BSP_SDCARD_TYPE_UNKNOWN;
    g_sd_status       = BSP_SDCARD_STATUS_NO_CARD;
    g_bsp_initialized = false;
    return false;
}

/**
 * @brief 读取单个扇区
 */
bsp_sdcard_result_t bsp_sdcard_read_sector(uint32_t sector, uint8_t* buf)
{
    uint8_t r1;

    if (buf == NULL) { return BSP_SDCARD_ERROR; }

    // 非SDHC卡需要转换地址
    if (g_sd_type != BSP_SDCARD_TYPE_V2HC) { sector *= 512; }

    // 发送读命令
    r1 = _bsp_sdcard_send_cmd(SD_CMD17, sector, 0xFF);
    if (r1 != 0) { return BSP_SDCARD_ERROR; }

    // 接收数据
    r1 = _bsp_sdcard_receive_data(buf, 512);
    bsp_sdcard_deselect();

    return (r1 == BSP_SDCARD_OK) ? BSP_SDCARD_OK : BSP_SDCARD_ERROR;
}

/**
 * @brief 写入单个扇区（移植自主工程SD_WriteDisk逻辑）
 */
bsp_sdcard_result_t bsp_sdcard_write_sector(uint32_t sector, const uint8_t* buf)
{
    uint8_t r1;

    if (buf == NULL) { return BSP_SDCARD_ERROR; }

    // 非SDHC卡需要转换地址
    if (g_sd_type != BSP_SDCARD_TYPE_V2HC) { sector *= 512; }

    // 发送写命令（CMD24，CRC=0x01）
    r1 = _bsp_sdcard_send_cmd(SD_CMD24, sector, 0X01);
    if (r1 == 0) { r1 = _bsp_sdcard_send_data(buf, 0xFE); }

    bsp_sdcard_deselect();
    return (r1 == 0) ? BSP_SDCARD_OK : BSP_SDCARD_ERROR;
}

/**
 * @brief 读取多个扇区
 */
bsp_sdcard_result_t bsp_sdcard_read_multi_sector(uint32_t sector, uint8_t* buf,
                                                 uint32_t count)
{
    if (buf == NULL || count == 0) { return BSP_SDCARD_ERROR; }

    // 简化实现：逐个读取
    for (uint32_t i = 0; i < count; i++) {
        if (bsp_sdcard_read_sector(sector + i, buf + i * 512) !=
            BSP_SDCARD_OK) {
            return BSP_SDCARD_ERROR;
        }
    }

    return BSP_SDCARD_OK;
}

/**
 * @brief 写入多个扇区
 */
bsp_sdcard_result_t bsp_sdcard_write_multi_sector(uint32_t       sector,
                                                  const uint8_t* buf,
                                                  uint32_t       count)
{
    if (buf == NULL || count == 0) { return BSP_SDCARD_ERROR; }

    // 逐个写入扇区
    for (uint32_t i = 0; i < count; i++) {
        if (bsp_sdcard_write_sector(sector + i, buf + i * 512) !=
            BSP_SDCARD_OK) {
            return BSP_SDCARD_ERROR;
        }
    }

    return BSP_SDCARD_OK;
}

/**
 * @brief 获取SD卡当前状态
 */
bsp_sdcard_status_t bsp_sdcard_get_status(void)
{
    return g_sd_status;
}

/**
 * @brief 获取SD卡类型
 */
bsp_sdcard_type_t bsp_sdcard_get_type(void)
{
    return g_sd_type;
}

/**
 * @brief 获取扇区总数
 */
uint32_t bsp_sdcard_get_sector_count(void)
{
    // 简化实现，实际需要读取CSD寄存器
    return 0;
}
