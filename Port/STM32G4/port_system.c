/**
  ******************************************************************************
  * @file    port_system.c
  * @brief   STM32G4平台系统接口移植层实现
  * @version V1.0.0
  * @date    2025-12-10
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "../port_def.h"
#include "main.h"
#include "stm32g4xx_ll_utils.h"

/* Private variables ---------------------------------------------------------*/

/* 临界区嵌套计数 */
static volatile uint32_t s_critical_nesting = 0;

/* 保存的中断状态 */
static volatile uint32_t s_primask_backup = 0;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief 获取系统时间戳（毫秒）
 * @return 当前时间戳(ms)
 */
uint32_t port_get_tick(void)
{
    return HAL_GetTick();
}

/**
 * @brief 毫秒延时
 * @param ms 延时毫秒数
 */
void port_delay_ms(uint32_t ms)
{
    HAL_Delay(ms);
}

/**
 * @brief 微秒延时
 * @param us 延时微秒数
 * @note  使用LL库的延时函数，精度取决于系统时钟配置
 *        对于80MHz主频，1us对应约80个时钟周期
 */
void port_delay_us(uint32_t us)
{
    /* 使用简单的循环延时，约每微秒执行一定次数 */
    /* 对于80MHz主频，每微秒约80个周期，考虑循环开销 */
    volatile uint32_t count = us * 10;  /* 调整系数以匹配实际延时 */
    while (count--)
    {
        __NOP();
    }
}

/**
 * @brief 进入临界区（禁用中断）
 * @note  支持嵌套调用
 */
void port_enter_critical(void)
{
    if (s_critical_nesting == 0)
    {
        /* 保存当前中断状态并禁用中断 */
        s_primask_backup = __get_PRIMASK();
        __disable_irq();
    }
    s_critical_nesting++;
}

/**
 * @brief 退出临界区（恢复中断）
 * @note  支持嵌套调用
 */
void port_exit_critical(void)
{
    if (s_critical_nesting > 0)
    {
        s_critical_nesting--;
        if (s_critical_nesting == 0)
        {
            /* 恢复之前的中断状态 */
            __set_PRIMASK(s_primask_backup);
        }
    }
}

