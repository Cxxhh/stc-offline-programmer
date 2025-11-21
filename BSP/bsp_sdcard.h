/**
 ******************************************************************************
 * @file    bsp_sdcard.h
 * @brief   BSP层SD卡硬件驱动接口（SPI模式）
 * @note    支持FatFs文件系统，提供按需挂载机制
 * @version V2.0.0
 * @date    2025-01-XX
 ******************************************************************************
 */

#ifndef BSP_SDCARD_H
#define BSP_SDCARD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "bsp_common.h"
#include "../Inc/main.h"  // For SD_CS_Pin definition

/* Exported constants --------------------------------------------------------*/

#define BSP_SDCARD_BLOCK_SIZE 512 ///< SD卡块大小（字节）

/* Exported types ------------------------------------------------------------*/

/**
 * @brief SD卡类型枚举
 */
typedef enum {
    BSP_SDCARD_TYPE_UNKNOWN = 0, ///< 未知类型
    BSP_SDCARD_TYPE_MMC     = 1, ///< MMC卡
    BSP_SDCARD_TYPE_V1      = 2, ///< SD V1.x标准卡
    BSP_SDCARD_TYPE_V2      = 3, ///< SD V2.0标准卡
    BSP_SDCARD_TYPE_V2HC    = 4  ///< SD V2.0高容量卡（SDHC）
} bsp_sdcard_type_t;

/**
 * @brief SD卡状态枚举
 */
typedef enum {
    BSP_SDCARD_STATUS_NO_CARD    = 0, ///< 无卡
    BSP_SDCARD_STATUS_CARD_READY = 1, ///< 卡已初始化，未挂载
    BSP_SDCARD_STATUS_MOUNTED    = 2, ///< 文件系统已挂载
    BSP_SDCARD_STATUS_ERROR      = 3  ///< 错误状态
} bsp_sdcard_status_t;

/**
 * @brief SD卡信息结构体
 */
typedef struct {
    bsp_sdcard_type_t type;         ///< SD卡类型
    uint32_t          capacity_mb;  ///< 容量（MB）
    uint16_t          block_size;   ///< 块大小（字节）
    uint32_t          sector_count; ///< 扇区总数
    bool              detected;     ///< 是否检测到卡
} bsp_sdcard_info_t;

/**
 * @brief SD卡操作状态码
 */
typedef enum {
    BSP_SDCARD_OK      = 0, ///< 操作成功
    BSP_SDCARD_ERROR   = 1, ///< 操作失败
    BSP_SDCARD_BUSY    = 2, ///< 卡忙
    BSP_SDCARD_TIMEOUT = 3  ///< 超时
} bsp_sdcard_result_t;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 初始化SD卡硬件
 * @return bsp_sdcard_result_t 初始化状态
 * @note 配置SPI接口和CS引脚
 */
bsp_sdcard_result_t bsp_sdcard_init(void);

/**
 * @brief 检测SD卡并读取信息
 * @param info SD卡信息结构体指针
 * @return bsp_sdcard_result_t 检测状态
 */
bsp_sdcard_result_t bsp_sdcard_detect(bsp_sdcard_info_t* info);

/**
 * @brief 检查SD卡是否插入
 * @return bool true=已插入, false=未插入
 */
bool                bsp_sdcard_is_inserted(void);

/**
 * @brief 快速检测是否存在SD卡（CMD0探测）
 * @return bool true=检测到卡, false=未检测到
 */
bool                bsp_sdcard_detect_fast(void);

/**
 * @brief 快速检测SD卡是否仍然在线（CMD13心跳）
 * @return bool true=正常, false=无响应
 */
bool                bsp_sdcard_check_alive(void);

/**
 * @brief 读取单个扇区
 * @param sector 扇区号
 * @param buf 数据缓冲区（至少512字节）
 * @return bsp_sdcard_result_t 操作状态
 */
bsp_sdcard_result_t bsp_sdcard_read_sector(uint32_t sector, uint8_t* buf);

/**
 * @brief 写入单个扇区
 * @param sector 扇区号
 * @param buf 数据缓冲区（512字节）
 * @return bsp_sdcard_result_t 操作状态
 */
bsp_sdcard_result_t bsp_sdcard_write_sector(uint32_t       sector,
                                            const uint8_t* buf);

/**
 * @brief 读取多个扇区
 * @param sector 起始扇区号
 * @param buf 数据缓冲区
 * @param count 扇区数量
 * @return bsp_sdcard_result_t 操作状态
 */
bsp_sdcard_result_t bsp_sdcard_read_multi_sector(uint32_t sector, uint8_t* buf,
                                                 uint32_t count);

/**
 * @brief 写入多个扇区
 * @param sector 起始扇区号
 * @param buf 数据缓冲区
 * @param count 扇区数量
 * @return bsp_sdcard_result_t 操作状态
 */
bsp_sdcard_result_t bsp_sdcard_write_multi_sector(uint32_t       sector,
                                                  const uint8_t* buf,
                                                  uint32_t       count);

/**
 * @brief 获取SD卡当前状态
 * @return bsp_sdcard_status_t 状态
 */
bsp_sdcard_status_t bsp_sdcard_get_status(void);

/**
 * @brief 获取SD卡类型
 * @return bsp_sdcard_type_t 卡类型
 */
bsp_sdcard_type_t   bsp_sdcard_get_type(void);

/**
 * @brief 获取扇区总数
 * @return uint32_t 扇区数量
 */
uint32_t            bsp_sdcard_get_sector_count(void);

/* Exported helper functions (for BSP layer) ---------------------------------*/

/**
 * @brief 取消选择SD卡（用于与W25Qxx互斥访问）
 * @note 由bsp_w25qxx.c调用
 */
void                bsp_sdcard_deselect(void);
// CS引脚控制
void                bsp_sdcard_select(void);
#ifdef __cplusplus
}
#endif

#endif // BSP_SDCARD_H
