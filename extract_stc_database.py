#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
STC单片机数据库提取工具
从stcgal的models.py中提取STC单片机型号数据，生成C语言格式的数据库文件
"""

import re
import sys
from pathlib import Path
from datetime import datetime

def parse_models_file(models_file_path):
    """解析models.py文件，提取所有MCUModel定义"""
    with open(models_file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # 使用正则表达式匹配MCUModel定义
    # MCUModel(name='STC32F12K16', magic=0xf871, total=55296, code=16384, eeprom=38912, iap=False, calibrate=True, mcs251=True)
    pattern = r"MCUModel\(name='([^']+)',\s*magic=(0x[0-9a-fA-F]+),\s*total=(\d+),\s*code=(\d+),\s*eeprom=(\d+),\s*iap=(True|False),\s*calibrate=(True|False),\s*mcs251=(True|False)\)"
    
    models = []
    for match in re.finditer(pattern, content):
        model = {
            'name': match.group(1),
            'magic': int(match.group(2), 16),
            'total': int(match.group(3)),
            'code': int(match.group(4)),
            'eeprom': int(match.group(5)),
            'iap': match.group(6) == 'True',
            'calibrate': match.group(7) == 'True',
            'mcs251': match.group(8) == 'True'
        }
        models.append(model)
    
    return models

def generate_header_file(models, output_path):
    """生成C语言头文件"""
    content = f"""/**
 * @file stc_mcu_database.h
 * @brief STC单片机型号数据库头文件
 * @note 本文件由脚本自动生成，请勿手动修改
 * @date {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 * 
 * 数据来源: stcgal (https://github.com/grigorig/stcgal)
 * 提取时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 * 提取型号数量: {len(models)}
 */

#ifndef STC_MCU_DATABASE_H
#define STC_MCU_DATABASE_H

#ifdef __cplusplus
extern "C" {{
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief STC单片机型号信息结构体
 */
typedef struct {{
    const char *name;        /**< 型号名称 */
    uint16_t magic;          /**< 型号识别码（魔术字） */
    uint32_t total_flash;    /**< 总Flash大小（字节） */
    uint32_t code_flash;     /**< 代码区Flash大小（字节） */
    uint32_t eeprom_flash;   /**< EEPROM区大小（字节） */
    bool iap_support;        /**< 是否支持IAP（在应用编程） */
    bool calibrate_support;  /**< 是否支持RC振荡器校准 */
    bool is_mcs251;          /**< 是否为MCS-251架构（否则为8051架构） */
}} stc_mcu_model_t;

/**
 * @brief STC单片机数据库数组大小
 */
#define STC_MCU_MODEL_COUNT {len(models)}

/**
 * @brief STC单片机数据库数组（所有型号）
 */
extern const stc_mcu_model_t stc_mcu_database[STC_MCU_MODEL_COUNT];

/**
 * @brief 通过魔术字查找STC单片机型号
 * @param magic 型号识别码
 * @return 找到的型号指针，未找到返回NULL
 */
const stc_mcu_model_t* stc_find_model_by_magic(uint16_t magic);

/**
 * @brief 通过名称查找STC单片机型号
 * @param name 型号名称（大小写敏感）
 * @return 找到的型号指针，未找到返回NULL
 */
const stc_mcu_model_t* stc_find_model_by_name(const char *name);

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
}}
#endif

#endif /* STC_MCU_DATABASE_H */
"""
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"已生成头文件: {output_path}")

def generate_source_file(models, output_path):
    """生成C语言源文件"""
    # 生成数据库数组
    db_entries = []
    for model in models:
        entry = f"""    {{
        .name = "{model['name']}",
        .magic = 0x{model['magic']:04X},
        .total_flash = {model['total']},
        .code_flash = {model['code']},
        .eeprom_flash = {model['eeprom']},
        .iap_support = {'true' if model['iap'] else 'false'},
        .calibrate_support = {'true' if model['calibrate'] else 'false'},
        .is_mcs251 = {'true' if model['mcs251'] else 'false'}
    }}"""
        db_entries.append(entry)
    
    db_array = ",\n".join(db_entries)
    
    content = f"""/**
 * @file stc_mcu_database.c
 * @brief STC单片机型号数据库实现
 * @note 本文件由脚本自动生成，请勿手动修改
 * @date {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 * 
 * 数据来源: stcgal (https://github.com/grigorig/stcgal)
 * 提取时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 * 提取型号数量: {len(models)}
 */

#include "stc_mcu_database.h"
#include <string.h>

/**
 * @brief STC单片机完整数据库
 * 包含所有已知的STC单片机型号信息
 */
const stc_mcu_model_t stc_mcu_database[STC_MCU_MODEL_COUNT] = {{
{db_array}
}};

/**
 * @brief 通过魔术字查找STC单片机型号
 */
const stc_mcu_model_t* stc_find_model_by_magic(uint16_t magic)
{{
    for (uint32_t i = 0; i < STC_MCU_MODEL_COUNT; i++) {{
        if (stc_mcu_database[i].magic == magic) {{
            return &stc_mcu_database[i];
        }}
    }}
    return NULL;
}}

/**
 * @brief 通过名称查找STC单片机型号
 */
const stc_mcu_model_t* stc_find_model_by_name(const char *name)
{{
    if (name == NULL) {{
        return NULL;
    }}
    
    for (uint32_t i = 0; i < STC_MCU_MODEL_COUNT; i++) {{
        if (strcmp(stc_mcu_database[i].name, name) == 0) {{
            return &stc_mcu_database[i];
        }}
    }}
    return NULL;
}}

/**
 * @brief 打印STC单片机型号信息（需要实现printf）
 */
void stc_print_model_info(const stc_mcu_model_t *model)
{{
    if (model == NULL) {{
        return;
    }}
    
    // 注意：这里假设已经有printf实现，如果使用STM32可能需要重定向printf
    // 或者使用其他日志输出方式
    #ifdef USE_PRINTF
    printf("STC MCU Model Information:\\n");
    printf("  Name: %s\\n", model->name);
    printf("  Magic: 0x%04X\\n", model->magic);
    printf("  Total Flash: %lu bytes (%.1f KB)\\n", 
           model->total_flash, model->total_flash / 1024.0f);
    printf("  Code Flash: %lu bytes (%.1f KB)\\n", 
           model->code_flash, model->code_flash / 1024.0f);
    printf("  EEPROM Flash: %lu bytes (%.1f KB)\\n", 
           model->eeprom_flash, model->eeprom_flash / 1024.0f);
    printf("  IAP Support: %s\\n", model->iap_support ? "Yes" : "No");
    printf("  Calibrate Support: %s\\n", model->calibrate_support ? "Yes" : "No");
    printf("  Architecture: %s\\n", model->is_mcs251 ? "MCS-251" : "8051");
    #endif
}}

/**
 * @brief 获取数据库中的型号总数
 */
uint32_t stc_get_model_count(void)
{{
    return STC_MCU_MODEL_COUNT;
}}
"""
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print(f"已生成源文件: {output_path}")

def generate_statistics(models):
    """生成数据库统计信息"""
    stats = {
        'total_models': len(models),
        'mcs251_count': sum(1 for m in models if m['mcs251']),
        'i8051_count': sum(1 for m in models if not m['mcs251']),
        'iap_support_count': sum(1 for m in models if m['iap']),
        'calibrate_support_count': sum(1 for m in models if m['calibrate']),
    }
    
    # 统计各系列型号数量
    series_count = {}
    for model in models:
        # 提取系列名（如STC32、STC8H、STC15F等）
        name = model['name']
        if name.startswith('STC'):
            prefix = name[:5] if len(name) >= 5 else name[:4]
            # 进一步简化
            if prefix.startswith('STC32'):
                series = 'STC32'
            elif prefix.startswith('STC8'):
                series = 'STC8'
            elif prefix.startswith('STC15'):
                series = 'STC15'
            elif prefix.startswith('STC12'):
                series = 'STC12'
            elif prefix.startswith('STC11'):
                series = 'STC11'
            elif prefix.startswith('STC10'):
                series = 'STC10'
            elif prefix.startswith('STC90') or prefix.startswith('STC89'):
                series = 'STC89/90'
            else:
                series = 'Other'
        elif name.startswith('IAP'):
            series = 'IAP'
        elif name.startswith('IRC'):
            series = 'IRC'
        elif name.startswith('GX'):
            series = 'GX'
        else:
            series = 'Other'
        
        series_count[series] = series_count.get(series, 0) + 1
    
    return stats, series_count

def main():
    """主函数"""
    print("=" * 60)
    print("STC单片机数据库提取工具")
    print("=" * 60)
    
    # 设置路径
    script_dir = Path(__file__).parent
    models_file = script_dir / 'other' / 'stcgal-master' / 'stcgal' / 'models.py'
    output_header = script_dir / 'stc_mcu_database.h'
    output_source = script_dir / 'stc_mcu_database.c'
    
    if not models_file.exists():
        print(f"错误: 找不到models.py文件: {models_file}")
        sys.exit(1)
    
    print(f"\n正在解析: {models_file}")
    models = parse_models_file(models_file)
    print(f"成功提取: {len(models)} 个STC单片机型号")
    
    # 生成统计信息
    stats, series_count = generate_statistics(models)
    print("\n数据库统计信息:")
    print(f"  总型号数: {stats['total_models']}")
    print(f"  MCS-251架构: {stats['mcs251_count']}")
    print(f"  8051架构: {stats['i8051_count']}")
    print(f"  支持IAP: {stats['iap_support_count']}")
    print(f"  支持校准: {stats['calibrate_support_count']}")
    
    print("\n各系列型号数量:")
    for series, count in sorted(series_count.items(), key=lambda x: x[1], reverse=True):
        print(f"  {series}: {count}")
    
    # 生成C文件
    print("\n正在生成C语言文件...")
    generate_header_file(models, output_header)
    generate_source_file(models, output_source)
    
    print("\n" + "=" * 60)
    print("数据库提取完成!")
    print("=" * 60)
    print(f"\n生成的文件:")
    print(f"  - {output_header}")
    print(f"  - {output_source}")
    print(f"\n使用方法:")
    print(f"  1. 将生成的.h和.c文件添加到STM32工程中")
    print(f"  2. 在代码中包含头文件: #include \"stc_mcu_database.h\"")
    print(f"  3. 使用查询函数查找STC型号:")
    print(f"     const stc_mcu_model_t *model = stc_find_model_by_magic(0xF871);")
    print(f"     if (model != NULL) {{ /* 使用model */ }}")

if __name__ == '__main__':
    main()
