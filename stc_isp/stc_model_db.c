/**
 * @file stc_model_db.c
 * @brief STC型号数据库和协议选择实现
 */

#include "stc_model_db.h"
#include <string.h>

/*============================================================================
 * 协议名称表（用于UI显示）
 *============================================================================*/
static const char* g_protocol_names[STC_PROTO_COUNT] = {
    "STC89/90系列",
    "STC89A系列",
    "STC10/11/12系列",
    "STC15A系列",
    "STC15系列",
    "STC8系列",
    "STC8H系列",
    "STC8H1K系列",
    "STC32系列",
    "STC15 USB",
};

/*============================================================================
 * 协议注册表
 *============================================================================*/
static const stc_protocol_entry_t g_protocol_registry[STC_PROTO_COUNT] = {
    {
        .id = STC_PROTO_STC89,
        .name = "STC89/90系列",
        .pattern = "STC89,STC90",
        .config = &stc_config_stc89,
        .ops = &stc89_protocol_ops,
    },
    {
        .id = STC_PROTO_STC89A,
        .name = "STC89A系列",
        .pattern = "STC12C5052,STC12LE5052",
        .config = &stc_config_stc89a,
        .ops = &stc89a_protocol_ops,
    },
    {
        .id = STC_PROTO_STC12,
        .name = "STC10/11/12系列",
        .pattern = "STC10,STC11,STC12,IAP10,IAP11,IAP12",
        .config = &stc_config_stc12,
        .ops = &stc12_protocol_ops,
    },
    {
        .id = STC_PROTO_STC15A,
        .name = "STC15A系列",
        .pattern = "STC15F10,STC15L10,STC15F20,STC15L20,IAP15F10,IAP15L10",
        .config = &stc_config_stc15a,
        .ops = &stc15a_protocol_ops,
    },
    {
        .id = STC_PROTO_STC15,
        .name = "STC15系列",
        .pattern = "STC15,IAP15,IRC15",
        .config = &stc_config_stc15,
        .ops = &stc15_protocol_ops,
    },
    {
        .id = STC_PROTO_STC8,
        .name = "STC8系列",
        .pattern = "STC8A,STC8F,STC8C,STC8G",
        .config = &stc_config_stc8,
        .ops = &stc8_protocol_ops,
    },
    {
        .id = STC_PROTO_STC8D,
        .name = "STC8H系列",
        .pattern = "STC8H",
        .config = &stc_config_stc8d,
        .ops = &stc8d_protocol_ops,
    },
    {
        .id = STC_PROTO_STC8G,
        .name = "STC8H1K系列",
        .pattern = "STC8H1K",
        .config = &stc_config_stc8g,
        .ops = &stc8g_protocol_ops,
    },
    {
        .id = STC_PROTO_STC32,
        .name = "STC32系列",
        .pattern = "STC32",
        .config = &stc_config_stc32,
        .ops = &stc32_protocol_ops,
    },
    {
        .id = STC_PROTO_USB15,
        .name = "STC15 USB",
        .pattern = "USB",
        .config = &stc_config_usb15,
        .ops = &usb15_protocol_ops,
    },
};

/*============================================================================
 * 型号数据库
 * 注意：这是一个精简版的数据库，包含常用型号
 * 完整数据库可能包含数百个型号
 *============================================================================*/
static const stc_model_info_t g_model_db[] = {
    /* STC89系列 */
    { 0xE001, "STC89C51RC",     4096,   0, STC_PROTO_STC89 },
    { 0xE002, "STC89C52RC",     8192,   0, STC_PROTO_STC89 },
    { 0xE003, "STC89C53RC",    13312,   0, STC_PROTO_STC89 },
    { 0xE004, "STC89C54RD+",   16384,   0, STC_PROTO_STC89 },
    { 0xE006, "STC89C58RD+",   32768,   0, STC_PROTO_STC89 },
    { 0xE101, "STC89LE51RC",    4096,   0, STC_PROTO_STC89 },
    { 0xE102, "STC89LE52RC",    8192,   0, STC_PROTO_STC89 },
    { 0xE103, "STC89LE53RC",   13312,   0, STC_PROTO_STC89 },
    { 0xE104, "STC89LE54RD+",  16384,   0, STC_PROTO_STC89 },
    { 0xE106, "STC89LE58RD+",  32768,   0, STC_PROTO_STC89 },
    
    /* STC90系列 */
    { 0xE042, "STC90C52RC",     8192,   0, STC_PROTO_STC89 },
    { 0xE046, "STC90C58RD+",   32768,   0, STC_PROTO_STC89 },
    { 0xE142, "STC90LE52RC",    8192,   0, STC_PROTO_STC89 },
    { 0xE146, "STC90LE58RD+",  32768,   0, STC_PROTO_STC89 },
    
    /* STC12系列 */
    { 0xD102, "STC12C5052",     5120,   0, STC_PROTO_STC89A },
    { 0xD162, "STC12LE5052",    5120,   0, STC_PROTO_STC89A },
    { 0xD172, "STC12C5052AD",   5120,   0, STC_PROTO_STC89A },
    { 0xD1F2, "STC12LE5052AD",  5120,   0, STC_PROTO_STC89A },
    
    { 0xD164, "STC12C5A60S2",  61440, 1024, STC_PROTO_STC12 },
    { 0xD168, "STC12C5A56S2",  57344, 1024, STC_PROTO_STC12 },
    { 0xD16C, "STC12C5A52S2",  53248, 1024, STC_PROTO_STC12 },
    { 0xD170, "STC12C5A48S2",  49152, 1024, STC_PROTO_STC12 },
    { 0xD174, "STC12C5A40S2",  40960, 1024, STC_PROTO_STC12 },
    { 0xD178, "STC12C5A32S2",  32768, 1024, STC_PROTO_STC12 },
    { 0xD17C, "STC12C5A16S2",  16384, 1024, STC_PROTO_STC12 },
    { 0xD180, "STC12C5A08S2",   8192, 1024, STC_PROTO_STC12 },
    
    { 0xD1E4, "STC12LE5A60S2", 61440, 1024, STC_PROTO_STC12 },
    { 0xD1E8, "STC12LE5A56S2", 57344, 1024, STC_PROTO_STC12 },
    { 0xD1EC, "STC12LE5A52S2", 53248, 1024, STC_PROTO_STC12 },
    { 0xD1F0, "STC12LE5A48S2", 49152, 1024, STC_PROTO_STC12 },
    
    /* STC15系列 */
    { 0xF410, "STC15F104E",     4096,   0, STC_PROTO_STC15A },
    { 0xF411, "STC15F104W",     4096,   0, STC_PROTO_STC15A },
    { 0xF440, "STC15L104E",     4096,   0, STC_PROTO_STC15A },
    { 0xF441, "STC15L104W",     4096,   0, STC_PROTO_STC15A },
    
    { 0xF449, "STC15W408AS",    8192, 4096, STC_PROTO_STC15 },
    { 0xF44D, "STC15W404AS",    4096, 4096, STC_PROTO_STC15 },
    { 0xF44E, "STC15W401AS",    1024, 4096, STC_PROTO_STC15 },
    { 0xF450, "STC15W4K64S4",  65536, 1024, STC_PROTO_STC15 },
    { 0xF451, "STC15W4K56S4",  57344, 1024, STC_PROTO_STC15 },
    { 0xF452, "STC15W4K48S4",  49152, 1024, STC_PROTO_STC15 },
    { 0xF453, "STC15W4K40S4",  40960, 1024, STC_PROTO_STC15 },
    { 0xF454, "STC15W4K32S4",  32768, 1024, STC_PROTO_STC15 },
    { 0xF455, "STC15W4K16S4",  16384, 1024, STC_PROTO_STC15 },
    
    { 0xF488, "IAP15W4K61S4",  61440, 1024, STC_PROTO_STC15 },
    { 0xF489, "IAP15W4K58S4",  59392, 1024, STC_PROTO_STC15 },
    
    /* STC8系列 */
    { 0xF730, "STC8A8K64S4A12", 65536, 1024, STC_PROTO_STC8 },
    { 0xF731, "STC8A8K60S4A12", 61440, 1024, STC_PROTO_STC8 },
    { 0xF732, "STC8A8K56S4A12", 57344, 1024, STC_PROTO_STC8 },
    { 0xF733, "STC8A8K52S4A12", 53248, 1024, STC_PROTO_STC8 },
    { 0xF734, "STC8A8K48S4A12", 49152, 1024, STC_PROTO_STC8 },
    { 0xF735, "STC8A8K32S4A12", 32768, 1024, STC_PROTO_STC8 },
    { 0xF736, "STC8A8K16S4A12", 16384, 1024, STC_PROTO_STC8 },
    
    { 0xF7A0, "STC8G1K08",       8192, 1024, STC_PROTO_STC8 },
    { 0xF7A1, "STC8G1K08A",      8192, 1024, STC_PROTO_STC8 },
    { 0xF7A4, "STC8G1K12",      12288, 1024, STC_PROTO_STC8 },
    { 0xF7A5, "STC8G1K12A",     12288, 1024, STC_PROTO_STC8 },
    { 0xF7A8, "STC8G1K17",      17408, 1024, STC_PROTO_STC8 },
    { 0xF7A9, "STC8G1K17A",     17408, 1024, STC_PROTO_STC8 },
    
    { 0xF7B0, "STC8G2K64S4",    65536, 1024, STC_PROTO_STC8 },
    { 0xF7B1, "STC8G2K60S4",    61440, 1024, STC_PROTO_STC8 },
    { 0xF7B2, "STC8G2K48S4",    49152, 1024, STC_PROTO_STC8 },
    { 0xF7B3, "STC8G2K32S4",    32768, 1024, STC_PROTO_STC8 },
    { 0xF7B4, "STC8G2K16S4",    16384, 1024, STC_PROTO_STC8 },
    
    /* STC8H系列 */
    { 0xF7C0, "STC8H1K08",       8192, 4096, STC_PROTO_STC8G },
    { 0xF7C1, "STC8H1K12",      12288, 4096, STC_PROTO_STC8G },
    { 0xF7C2, "STC8H1K17",      17408, 4096, STC_PROTO_STC8G },
    { 0xF7C3, "STC8H1K24",      24576, 4096, STC_PROTO_STC8G },
    { 0xF7C4, "STC8H1K28",      28672, 4096, STC_PROTO_STC8G },
    { 0xF7C5, "STC8H1K33",      33792, 4096, STC_PROTO_STC8G },
    
    { 0xF7D0, "STC8H3K64S4",    65536, 1024, STC_PROTO_STC8D },
    { 0xF7D1, "STC8H3K64S2",    65536, 1024, STC_PROTO_STC8D },
    { 0xF7D2, "STC8H3K60S4",    61440, 1024, STC_PROTO_STC8D },
    { 0xF7D3, "STC8H3K60S2",    61440, 1024, STC_PROTO_STC8D },
    { 0xF7D4, "STC8H3K48S4",    49152, 1024, STC_PROTO_STC8D },
    { 0xF7D5, "STC8H3K48S2",    49152, 1024, STC_PROTO_STC8D },
    { 0xF7D6, "STC8H3K32S4",    32768, 1024, STC_PROTO_STC8D },
    { 0xF7D7, "STC8H3K32S2",    32768, 1024, STC_PROTO_STC8D },
    
    { 0xF7E0, "STC8H8K64U",     65536, 1024, STC_PROTO_STC8D },
    { 0xF7E1, "STC8H8K60U",     61440, 1024, STC_PROTO_STC8D },
    { 0xF7E2, "STC8H8K48U",     49152, 1024, STC_PROTO_STC8D },
    { 0xF7E3, "STC8H8K32U",     32768, 1024, STC_PROTO_STC8D },
    
    /* STC32系列 */
    { 0xF800, "STC32G12K128",  131072, 4096, STC_PROTO_STC32 },
    { 0xF801, "STC32G11K128",  131072, 4096, STC_PROTO_STC32 },
    { 0xF802, "STC32G10K128",  131072, 4096, STC_PROTO_STC32 },
    { 0xF810, "STC32G8K64",     65536, 4096, STC_PROTO_STC32 },
    { 0xF811, "STC32G8K48",     49152, 4096, STC_PROTO_STC32 },
    { 0xF812, "STC32G8K32",     32768, 4096, STC_PROTO_STC32 },
};

#define MODEL_DB_SIZE  (sizeof(g_model_db) / sizeof(g_model_db[0]))

/*============================================================================
 * 字符串工具函数
 *============================================================================*/

int stc_str_starts_with(const char* str, const char* prefix)
{
    if (str == NULL || prefix == NULL) {
        return 0;
    }
    
    while (*prefix) {
        if (*str != *prefix) {
            return 0;
        }
        str++;
        prefix++;
    }
    return 1;
}

int stc_str_contains(const char* str, const char* substr)
{
    if (str == NULL || substr == NULL) {
        return 0;
    }
    
    while (*str) {
        const char* s = str;
        const char* p = substr;
        
        while (*p && *s == *p) {
            s++;
            p++;
        }
        
        if (*p == '\0') {
            return 1;
        }
        str++;
    }
    return 0;
}

/**
 * @brief 检查型号是否匹配模式
 * @param model_name 型号名称
 * @param pattern 模式字符串（逗号分隔的前缀列表）
 * @return 1匹配，0不匹配
 */
static int match_pattern(const char* model_name, const char* pattern)
{
    if (model_name == NULL || pattern == NULL) {
        return 0;
    }
    
    /* 复制pattern以便修改 */
    char pattern_copy[128];
    strncpy(pattern_copy, pattern, sizeof(pattern_copy) - 1);
    pattern_copy[sizeof(pattern_copy) - 1] = '\0';
    
    /* 按逗号分隔检查每个前缀 */
    char* token = pattern_copy;
    char* next;
    
    while (token && *token) {
        /* 跳过前导空格 */
        while (*token == ' ') token++;
        
        /* 找到下一个逗号 */
        next = token;
        while (*next && *next != ',') next++;
        
        if (*next == ',') {
            *next = '\0';
            next++;
        } else {
            next = NULL;
        }
        
        /* 检查前缀 */
        if (stc_str_starts_with(model_name, token)) {
            return 1;
        }
        
        token = next;
    }
    
    return 0;
}

/*============================================================================
 * 型号数据库查询
 *============================================================================*/

const stc_model_info_t* stc_find_model_by_magic(uint16_t magic)
{
    for (uint16_t i = 0; i < MODEL_DB_SIZE; i++) {
        if (g_model_db[i].magic == magic) {
            return &g_model_db[i];
        }
    }
    return NULL;
}

const stc_model_info_t* stc_find_model_by_name(const char* name)
{
    if (name == NULL) {
        return NULL;
    }
    
    for (uint16_t i = 0; i < MODEL_DB_SIZE; i++) {
        if (strcmp(g_model_db[i].name, name) == 0) {
            return &g_model_db[i];
        }
    }
    return NULL;
}

uint16_t stc_get_model_count(void)
{
    return MODEL_DB_SIZE;
}

const stc_model_info_t* stc_get_model_by_index(uint16_t index)
{
    if (index >= MODEL_DB_SIZE) {
        return NULL;
    }
    return &g_model_db[index];
}

/*============================================================================
 * 协议注册表查询
 *============================================================================*/

int stc_get_protocol_by_id(stc_protocol_id_t id,
                           const stc_protocol_config_t** config,
                           const stc_protocol_ops_t** ops)
{
    if (id >= STC_PROTO_COUNT) {
        return STC_ERR_INVALID_PARAM;
    }
    
    if (config != NULL) {
        *config = g_protocol_registry[id].config;
    }
    
    if (ops != NULL) {
        *ops = g_protocol_registry[id].ops;
    }
    
    return STC_OK;
}

int stc_match_protocol_by_name(const char* model_name,
                               const stc_protocol_config_t** config,
                               const stc_protocol_ops_t** ops,
                               stc_protocol_id_t* id)
{
    if (model_name == NULL) {
        return STC_ERR_INVALID_PARAM;
    }
    
    /* 按优先级顺序匹配（更具体的模式在前）*/
    /* 优先级：STC8H1K > STC8H > STC8 > STC15A > STC15 > STC12 > STC89A > STC89 */
    
    /* 首先检查STC32 */
    if (stc_str_starts_with(model_name, "STC32")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC32].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC32].ops;
        if (id) *id = STC_PROTO_STC32;
        return STC_OK;
    }
    
    /* 检查STC8H1K（最具体） */
    if (stc_str_starts_with(model_name, "STC8H1K")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC8G].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC8G].ops;
        if (id) *id = STC_PROTO_STC8G;
        return STC_OK;
    }
    
    /* 检查STC8H */
    if (stc_str_starts_with(model_name, "STC8H")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC8D].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC8D].ops;
        if (id) *id = STC_PROTO_STC8D;
        return STC_OK;
    }
    
    /* 检查STC8 */
    if (stc_str_starts_with(model_name, "STC8")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC8].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC8].ops;
        if (id) *id = STC_PROTO_STC8;
        return STC_OK;
    }
    
    /* 检查STC15A（15F10x, 15L10x, 15F20x, 15L20x） */
    if ((stc_str_starts_with(model_name, "STC15F10") || 
         stc_str_starts_with(model_name, "STC15L10") ||
         stc_str_starts_with(model_name, "STC15F20") || 
         stc_str_starts_with(model_name, "STC15L20") ||
         stc_str_starts_with(model_name, "IAP15F10") ||
         stc_str_starts_with(model_name, "IAP15L10"))) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC15A].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC15A].ops;
        if (id) *id = STC_PROTO_STC15A;
        return STC_OK;
    }
    
    /* 检查STC15 */
    if (stc_str_starts_with(model_name, "STC15") ||
        stc_str_starts_with(model_name, "IAP15") ||
        stc_str_starts_with(model_name, "IRC15")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC15].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC15].ops;
        if (id) *id = STC_PROTO_STC15;
        return STC_OK;
    }
    
    /* 检查STC12C5052/STC12LE5052（使用STC89A协议） */
    if (stc_str_contains(model_name, "5052")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC89A].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC89A].ops;
        if (id) *id = STC_PROTO_STC89A;
        return STC_OK;
    }
    
    /* 检查STC10/11/12 */
    if (stc_str_starts_with(model_name, "STC10") ||
        stc_str_starts_with(model_name, "STC11") ||
        stc_str_starts_with(model_name, "STC12") ||
        stc_str_starts_with(model_name, "IAP10") ||
        stc_str_starts_with(model_name, "IAP11") ||
        stc_str_starts_with(model_name, "IAP12")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC12].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC12].ops;
        if (id) *id = STC_PROTO_STC12;
        return STC_OK;
    }
    
    /* 检查STC89/90 */
    if (stc_str_starts_with(model_name, "STC89") ||
        stc_str_starts_with(model_name, "STC90")) {
        if (config) *config = g_protocol_registry[STC_PROTO_STC89].config;
        if (ops) *ops = g_protocol_registry[STC_PROTO_STC89].ops;
        if (id) *id = STC_PROTO_STC89;
        return STC_OK;
    }
    
    return STC_ERR_UNKNOWN_MODEL;
}

int stc_get_protocol_list(const char*** names, int* count)
{
    if (names != NULL) {
        *names = g_protocol_names;
    }
    if (count != NULL) {
        *count = STC_PROTO_COUNT;
    }
    return STC_OK;
}

const char* stc_get_protocol_name(stc_protocol_id_t id)
{
    if (id >= STC_PROTO_COUNT) {
        return NULL;
    }
    return g_protocol_names[id];
}

const stc_protocol_entry_t* stc_get_protocol_entry(stc_protocol_id_t id)
{
    if (id >= STC_PROTO_COUNT) {
        return NULL;
    }
    return &g_protocol_registry[id];
}

