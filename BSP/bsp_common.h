/**
 ******************************************************************************
 * @file    bsp_common.h
 * @brief   BSP层公共定义和类型
 * @note    提供所有BSP模块共享的基础定义
 * @version V2.0.0
 * @date    2025-01-XX
 ******************************************************************************
 */

#ifndef BSP_COMMON_H
#define BSP_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f1xx_hal.h"

/* Exported constants --------------------------------------------------------*/

// BSP版本信息
#define BSP_VERSION_MAJOR 2
#define BSP_VERSION_MINOR 0
#define BSP_VERSION_PATCH 0

// 硬件配置参数
#define BSP_LCD_WIDTH   160  ///< LCD宽度（像素）
#define BSP_LCD_HEIGHT  128  ///< LCD高度（像素）
#define BSP_SPI_TIMEOUT 100  ///< SPI超时时间（毫秒）

/* Exported types ------------------------------------------------------------*/

/**
 * @brief BSP通用返回码
 */
typedef enum {
    BSP_OK       = 0,  ///< 操作成功
    BSP_ERROR    = 1,  ///< 操作失败
    BSP_BUSY     = 2,  ///< 设备忙
    BSP_TIMEOUT  = 3   ///< 超时
} bsp_status_t;

// RGB565颜色类型
typedef uint16_t bsp_color_t;

// 坐标点结构
typedef struct {
    uint16_t x;
    uint16_t y;
} bsp_point_t;

// 矩形区域结构
typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
} bsp_rect_t;

/* Exported macros -----------------------------------------------------------*/

// 编译器兼容性宏
#ifndef BSP_INLINE
    #ifdef __GNUC__
        #define BSP_INLINE static inline __attribute__((always_inline))
    #elif defined(__ARMCC_VERSION)
        #define BSP_INLINE static __inline
    #else
        #define BSP_INLINE static inline
    #endif
#endif

// 未使用参数宏（消除警告）
#define BSP_UNUSED(x) ((void)(x))

// 最小/最大值宏
#ifndef BSP_MIN
    #define BSP_MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef BSP_MAX
    #define BSP_MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

// 约束值到范围
#define BSP_CONSTRAIN(val, min, max) BSP_MAX(BSP_MIN((val), (max)), (min))

// 位操作宏
#define BSP_BIT_SET(reg, bit)     ((reg) |= (bit))
#define BSP_BIT_CLEAR(reg, bit)   ((reg) &= ~(bit))
#define BSP_BIT_TOGGLE(reg, bit)  ((reg) ^= (bit))
#define BSP_BIT_READ(reg, bit)    (((reg) & (bit)) != 0)

// 数组长度宏
#define BSP_ARRAY_SIZE(arr)  (sizeof(arr) / sizeof((arr)[0]))

// 延时宏（需要HAL_Delay）
#define BSP_DELAY_MS(ms)  HAL_Delay(ms)

/* Exported function macros --------------------------------------------------*/

// 安全指针检查宏
#define BSP_CHECK_NULL(ptr)  do { if ((ptr) == NULL) return BSP_ERROR; } while(0)

// 参数验证宏
#define BSP_ASSERT(expr)  do { if (!(expr)) return BSP_ERROR; } while(0)

#ifdef __cplusplus
}
#endif

#endif // BSP_COMMON_H

