/**
 * @file stc_mcu_database.h
 * @brief STC单片机型号数据库头文件
 * @note 本文件由脚本自动生成，请勿手动修改
 * @date 2025-11-28 19:09:46
 *
 * 数据来源: stcgal (https://github.com/grigorig/stcgal)
 * 提取时间: 2025-11-28 19:09:46
 * 提取型号数量: 1140
 */

#ifndef STC_MCU_DATABASE_H
#define STC_MCU_DATABASE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

    /**
     * @brief STC单片机型号信息结构体
     */
    typedef struct
    {
        const char *name;       /**< 型号名称 */
        uint16_t magic;         /**< 型号识别码（魔术字） */
        uint32_t total_flash;   /**< 总Flash大小（字节） */
        uint32_t code_flash;    /**< 代码区Flash大小（字节） */
        uint32_t eeprom_flash;  /**< EEPROM区大小（字节） */
        bool iap_support;       /**< 是否支持IAP（在应用编程） */
        bool calibrate_support; /**< 是否支持RC振荡器校准 */
        bool is_mcs251;         /**< 是否为MCS-251架构（否则为8051架构） */
    } stc_mcu_model_t;

/**
 * @brief STC单片机数据库数组大小
 */
#define STC_MCU_MODEL_COUNT 1140

    /**
     * @brief STC单片机数据库数组（所有型号）
     */
    extern const stc_mcu_model_t stc_mcu_database[STC_MCU_MODEL_COUNT];

    /**
     * @brief 通过魔术字查找STC单片机型号
     * @param magic 型号识别码
     * @return 找到的型号指针，未找到返回NULL
     */
    const stc_mcu_model_t *stc_find_model_by_magic(uint16_t magic);

    /**
     * @brief 通过名称查找STC单片机型号
     * @param name 型号名称（大小写敏感）
     * @return 找到的型号指针，未找到返回NULL
     */
    const stc_mcu_model_t *stc_find_model_by_name(const char *name);

    /**
     * @brief 打印STC单片机型号信息
     * @param model 型号指针
     */
    void stc_print_model_info(const stc_mcu_model_t *model);

    /**
     * @brief 获取数据库中的型号总数
     * @return 型号总数
     */
    uint32_t stc_get_model_count(void);

#ifdef __cplusplus
}
#endif

#endif /* STC_MCU_DATABASE_H */
