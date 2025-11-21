/**
 * @file diskio_sdcard.c
 * @brief Low level disk I/O module for SD card (IAP Bootloader)
 * @note Implements FatFs diskio interface for SD card via bsp_sdcard driver
 */

#include "diskio.h"
#include "../../BSP/bsp_sdcard.h"

/* Definitions of physical drive number for each drive */
#define DEV_SD          0       /* SD card */

/**
 * @brief Get Drive Status
 */
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != DEV_SD) {
        return STA_NOINIT;
    }
    
    // Check if SD card is ready
    if (bsp_sdcard_is_inserted()) {
        return 0;  // Ready
    }
    
    return STA_NODISK;
}

/**
 * @brief Initialize a Drive
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != DEV_SD) {
        return STA_NOINIT;
    }
    
    // SD card should already be initialized before mounting
    // This just checks status
    if (bsp_sdcard_is_inserted()) {
        return 0;  // Success
    }
    
    return STA_NOINIT;
}

/**
 * @brief Read Sector(s)
 */
DRESULT disk_read(
    BYTE pdrv,      /* Physical drive number */
    BYTE *buff,     /* Data buffer to store read data */
    DWORD sector,   /* Start sector in LBA */
    UINT count      /* Number of sectors to read */
)
{
    bsp_sdcard_result_t result;
    
    if (pdrv != DEV_SD || count == 0) {
        return RES_PARERR;
    }
    
    // Read single or multiple sectors
    if (count == 1) {
        result = bsp_sdcard_read_sector(sector, buff);
    } else {
        result = bsp_sdcard_read_multi_sector(sector, buff, count);
    }
    
    if (result == BSP_SDCARD_OK) {
        return RES_OK;
    }
    
    return RES_ERROR;
}

/**
 * @brief Write Sector(s) - Read-only for IAP
 */
DRESULT disk_write(
    BYTE pdrv,          /* Physical drive number */
    const BYTE *buff,   /* Data to be written */
    DWORD sector,       /* Start sector in LBA */
    UINT count          /* Number of sectors to write */
)
{
    bsp_sdcard_result_t result;
    
    if (pdrv != DEV_SD || count == 0) {
        return RES_PARERR;
    }
    
    if (!bsp_sdcard_is_inserted()) {
        return RES_ERROR;
    }
    
    if (count == 1) {
        result = bsp_sdcard_write_sector(sector, buff);
    } else {
        result = bsp_sdcard_write_multi_sector(sector, buff, count);
    }
    
    return (result == BSP_SDCARD_OK) ? RES_OK : RES_ERROR;
}

/**
 * @brief Miscellaneous Functions
 */
DRESULT disk_ioctl(
    BYTE pdrv,      /* Physical drive number */
    BYTE cmd,       /* Control code */
    void *buff      /* Buffer to send/receive control data */
)
{
    if (pdrv != DEV_SD) {
        return RES_PARERR;
    }
    
    switch (cmd) {
        case CTRL_SYNC:
            // Sync not needed for SD card (no write cache)
            return RES_OK;
            
        case GET_SECTOR_COUNT:
            // Return sector count
            *(DWORD*)buff = bsp_sdcard_get_sector_count();
            return RES_OK;
            
        case GET_SECTOR_SIZE:
            // SD card sector size is always 512
            *(WORD*)buff = 512;
            return RES_OK;
            
        case GET_BLOCK_SIZE:
            // Erase block size (not used in IAP, return 1)
            *(DWORD*)buff = 1;
            return RES_OK;
            
        default:
            return RES_PARERR;
    }
}

/**
 * @brief Get current time (fixed time for IAP)
 * @note Returns 2025-01-01 00:00:00
 */
DWORD get_fattime(void)
{
    // Return a fixed time (2025-01-01 00:00:00)
    // Bit 31-25: Year (2025 - 1980 = 45)
    // Bit 24-21: Month (1)
    // Bit 20-16: Day (1)
    // Bit 15-11: Hour (0)
    // Bit 10-5:  Minute (0)
    // Bit 4-0:   Second/2 (0)
    return ((DWORD)(2025 - 1980) << 25)
         | ((DWORD)1 << 21)
         | ((DWORD)1 << 16);
}
