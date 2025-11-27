/**
 * @file stc_model_db.h
 * @brief STC型号数据库和协议选择
 */

#ifndef __STC_MODEL_DB_H__
#define __STC_MODEL_DB_H__

#include "stc_types.h"
#include "stc_protocol_config.h"
#include "stc_protocol_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 型号信息结构体
 *============================================================================*/
typedef struct {
    uint16_t        magic;          // MCU Magic值（型号识别码）
    const char*     name;           // 型号名称
    uint32_t        flash_size;     // Flash大小（字节）
    uint32_t        eeprom_size;    // EEPROM大小（字节）
    stc_protocol_id_t protocol_id;  // 对应的协议ID
} stc_model_info_t;

/*============================================================================
 * 协议注册表项
 *============================================================================*/
typedef struct {
    stc_protocol_id_t                   id;         // 协议ID
    const char*                         name;       // 协议显示名称
    const char*                         pattern;    // 型号匹配模式（简化版）
    const stc_protocol_config_t*        config;     // 协议配置
    const stc_protocol_ops_t*           ops;        // 协议操作
} stc_protocol_entry_t;

/*============================================================================
 * 型号数据库查询
 *============================================================================*/

/**
 * @brief 根据Magic值查找型号信息
 * @param magic MCU Magic值
 * @return 型号信息指针，NULL表示未找到
 */
const stc_model_info_t* stc_find_model_by_magic(uint16_t magic);

/**
 * @brief 根据型号名称查找型号信息
 * @param name 型号名称
 * @return 型号信息指针，NULL表示未找到
 */
const stc_model_info_t* stc_find_model_by_name(const char* name);

/**
 * @brief 获取型号数据库条目数
 * @return 条目数
 */
uint16_t stc_get_model_count(void);

/**
 * @brief 获取型号数据库指定索引的型号
 * @param index 索引
 * @return 型号信息指针，NULL表示索引无效
 */
const stc_model_info_t* stc_get_model_by_index(uint16_t index);

/*============================================================================
 * 协议注册表查询
 *============================================================================*/

/**
 * @brief 根据协议ID获取协议
 * @param id 协议ID
 * @param config 输出协议配置
 * @param ops 输出协议操作
 * @return STC_OK成功，其他失败
 */
int stc_get_protocol_by_id(stc_protocol_id_t id,
                           const stc_protocol_config_t** config,
                           const stc_protocol_ops_t** ops);

/**
 * @brief 根据型号名称匹配协议
 * @param model_name 型号名称
 * @param config 输出协议配置
 * @param ops 输出协议操作
 * @param id 输出协议ID
 * @return STC_OK成功，其他失败
 */
int stc_match_protocol_by_name(const char* model_name,
                               const stc_protocol_config_t** config,
                               const stc_protocol_ops_t** ops,
                               stc_protocol_id_t* id);

/**
 * @brief 获取协议列表（用于UI显示）
 * @param names 输出协议名称数组指针
 * @param count 输出协议数量
 * @return STC_OK成功
 */
int stc_get_protocol_list(const char*** names, int* count);

/**
 * @brief 获取协议显示名称
 * @param id 协议ID
 * @return 协议名称，NULL表示ID无效
 */
const char* stc_get_protocol_name(stc_protocol_id_t id);

/**
 * @brief 获取协议注册表项
 * @param id 协议ID
 * @return 协议注册表项指针，NULL表示ID无效
 */
const stc_protocol_entry_t* stc_get_protocol_entry(stc_protocol_id_t id);

/*============================================================================
 * 简化的型号匹配（用于嵌入式环境，不使用正则）
 *============================================================================*/

/**
 * @brief 简单前缀匹配
 * @param str 字符串
 * @param prefix 前缀
 * @return 1匹配，0不匹配
 */
int stc_str_starts_with(const char* str, const char* prefix);

/**
 * @brief 检查字符串是否包含子串
 * @param str 字符串
 * @param substr 子串
 * @return 1包含，0不包含
 */
int stc_str_contains(const char* str, const char* substr);

#ifdef __cplusplus
}
#endif

#endif /* __STC_MODEL_DB_H__ */

