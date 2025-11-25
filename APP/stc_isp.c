/**
 ******************************************************************************
 * @file    stc_isp.c
 * @brief   STC8 ISP handshake helper implementation (status read only)
 ******************************************************************************
 * - Uses USART in blocking mode with LL drivers.
 * - Power is toggled via PA11 to force cold start.
 * - Handshake runs at 2400 baud, even parity.
 ******************************************************************************
 */

#include "stc_isp.h"
#include "lcd.h"
#include <string.h>
#include <stdio.h>

static USART_TypeDef *stc_uart_instance = NULL;

static uint16_t STC_ISP_CalculateChecksum(const uint8_t *data, uint16_t length);
static STC_ISP_StatusTypeDef STC_ISP_ReadByte(USART_TypeDef *uart_instance, uint8_t *byte, uint32_t timeout_ms);
static void STC_ISP_DelayMs(uint32_t ms);
static uint32_t STC_ISP_GetUartClockFreq(USART_TypeDef *uart_instance);
static void STC_ISP_UpdateLCDStatus(const char *status);

static uint16_t STC_ISP_CalculateChecksum(const uint8_t *data, uint16_t length)
{
    uint16_t checksum = 0;
    for (uint16_t i = 0; i < length; i++)
    {
        checksum += data[i];
    }
    return (uint16_t)(checksum & 0xFFFF);
}

static STC_ISP_StatusTypeDef STC_ISP_ReadByte(USART_TypeDef *uart_instance, uint8_t *byte, uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();

    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        if (LL_USART_IsActiveFlag_RXNE(uart_instance))
        {
            *byte = LL_USART_ReceiveData8(uart_instance);
            return STC_ISP_OK;
        }
    }

    return STC_ISP_ERROR_TIMEOUT;
}

static void STC_ISP_DelayMs(uint32_t ms)
{
    HAL_Delay(ms);
}

static uint32_t STC_ISP_GetUartClockFreq(USART_TypeDef *uart_instance)
{
    if (uart_instance == USART1)
    {
        return LL_RCC_GetUSARTClockFreq(LL_RCC_USART1_CLKSOURCE);
    }
    if (uart_instance == USART2)
    {
        return LL_RCC_GetUSARTClockFreq(LL_RCC_USART2_CLKSOURCE);
    }
    if (uart_instance == USART3)
    {
        return LL_RCC_GetUSARTClockFreq(LL_RCC_USART3_CLKSOURCE);
    }
    /* Fallback */
    return SystemCoreClock;
}

static void STC_ISP_UpdateLCDStatus(const char *status)
{
    char display_line[21] = {0};
    LCD_SetTextColor(White);
    LCD_SetBackColor(Black);
    LCD_DisplayStringLine(Line0, (u8 *)"STC ISP Handshake");

    /* Format status string to fit LCD (max 20 chars) */
    snprintf(display_line, sizeof(display_line), "%-20s", status);
    LCD_DisplayStringLine(Line1, (u8 *)display_line);

    /* Clear next line for additional info */
    LCD_ClearLine(Line2);

    /* Small delay to ensure LCD update is visible */
    HAL_Delay(10);
}

STC_ISP_StatusTypeDef STC_ISP_ReconfigureUart(USART_TypeDef *uart_instance, uint32_t baudrate, uint32_t parity)
{
    if (uart_instance == NULL)
    {
        return STC_ISP_ERROR_PROTOCOL;
    }

    LL_USART_InitTypeDef USART_InitStruct;
    LL_USART_StructInit(&USART_InitStruct);
    USART_InitStruct.BaudRate = baudrate;
    USART_InitStruct.DataWidth = LL_USART_DATAWIDTH_8B;
    USART_InitStruct.StopBits = LL_USART_STOPBITS_1;
    USART_InitStruct.Parity = parity;
    USART_InitStruct.TransferDirection = LL_USART_DIRECTION_TX_RX;
    USART_InitStruct.HardwareFlowControl = LL_USART_HWCONTROL_NONE;
    USART_InitStruct.OverSampling = LL_USART_OVERSAMPLING_16;

    LL_USART_Disable(uart_instance);
    
    // 清空接收缓冲区
    while (LL_USART_IsActiveFlag_RXNE(uart_instance))
    {
        LL_USART_ReceiveData8(uart_instance);
    }
    
    LL_USART_Init(uart_instance, &USART_InitStruct);
    LL_USART_SetBaudRate(uart_instance,
                         STC_ISP_GetUartClockFreq(uart_instance),
                         LL_USART_PRESCALER_DIV1,
                         LL_USART_OVERSAMPLING_16,
                         baudrate);
    LL_USART_ConfigAsyncMode(uart_instance);
    LL_USART_Enable(uart_instance);

    // 等待UART就绪
    while ((!LL_USART_IsActiveFlag_TEACK(uart_instance)) || (!LL_USART_IsActiveFlag_REACK(uart_instance)))
    {
    }
    
    // 再次清空接收缓冲区，确保没有残留数据
    while (LL_USART_IsActiveFlag_RXNE(uart_instance))
    {
        LL_USART_ReceiveData8(uart_instance);
    }

    return STC_ISP_OK;
}

STC_ISP_StatusTypeDef STC_ISP_Init(USART_TypeDef *uart_instance)
{
    if (uart_instance == NULL)
    {
        return STC_ISP_ERROR_PROTOCOL;
    }

    stc_uart_instance = uart_instance;

    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};
    LL_AHB2_GRP1_EnableClock(LL_AHB2_GRP1_PERIPH_GPIOA);

    GPIO_InitStruct.Pin = STC_POWER_CTRL_PIN;
    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_HIGH;  // 提高驱动速度，增强驱动能力
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;  // 使用开漏输出
    GPIO_InitStruct.Pull = LL_GPIO_PULL_DOWN;  // 使用下拉电阻，确保LOW时为0V
    LL_GPIO_Init(STC_POWER_CTRL_PORT, &GPIO_InitStruct);

    /* Default power on (开漏模式下，设置为高电平=释放，由MOS开关电路控制) */
    LL_GPIO_SetOutputPin(STC_POWER_CTRL_PORT, STC_POWER_CTRL_PIN);

    /* Initialize LCD display */
    STC_ISP_UpdateLCDStatus("Ready...");

    return STC_ISP_OK;
}

void STC_ISP_ColdStart(void)
{
    STC_ISP_UpdateLCDStatus("Step 1: Cold Start...");
    LL_GPIO_ResetOutputPin(STC_POWER_CTRL_PORT, STC_POWER_CTRL_PIN);
    STC_ISP_DelayMs(3000);  // 拉低3秒，等待缓启动电路完全断电

    LL_GPIO_SetOutputPin(STC_POWER_CTRL_PORT, STC_POWER_CTRL_PIN);
    // 上电后立即返回，不等待，让同步脉冲发送在后台进行
    STC_ISP_UpdateLCDStatus("Step 1: Cold Start OK");
}

STC_ISP_StatusTypeDef STC_ISP_SendSyncPulse(USART_TypeDef *uart_instance, uint32_t timeout_ms)
{
    uint32_t start_tick = HAL_GetTick();
    uint8_t response_byte = 0;
    uint32_t last_send_tick = 0;

    // 清空接收缓冲区
    while (LL_USART_IsActiveFlag_RXNE(uart_instance))
    {
        LL_USART_ReceiveData8(uart_instance);
    }

    while ((HAL_GetTick() - start_tick) < timeout_ms)
    {
        // 每30ms发送一次0x7F（根据stcgal协议）
        uint32_t current_tick = HAL_GetTick();
        if ((current_tick - last_send_tick) >= 30)
        {
            // 等待发送寄存器为空
            while (!LL_USART_IsActiveFlag_TXE(uart_instance))
            {
            }
            
            // 发送同步字符 0x7F
            LL_USART_TransmitData8(uart_instance, STC_SYNC_CHAR);
            
            // 等待传输完成（TC标志）
            while (!LL_USART_IsActiveFlag_TC(uart_instance))
            {
            }
            
            last_send_tick = current_tick;
        }
        
        // 检查是否有响应（不等待，立即检查）
        if (LL_USART_IsActiveFlag_RXNE(uart_instance))
        {
            response_byte = LL_USART_ReceiveData8(uart_instance);
            if ((response_byte == STC_PACKET_MCU) || (response_byte == STC_PACKET_START_0))
            {
                return STC_ISP_OK;
            }
        }
        
        // 短暂延时，避免CPU占用过高
        HAL_Delay(1);
    }

    return STC_ISP_ERROR_TIMEOUT;
}

STC_ISP_StatusTypeDef STC_ISP_ReadStatusPacket(USART_TypeDef *uart_instance,
                                               STC_StatusPacketTypeDef *status_packet,
                                               uint32_t timeout_ms)
{
    uint8_t byte = 0;
    uint8_t len_high = 0;
    uint8_t len_low = 0;
    uint16_t packet_length = 0;
    uint16_t payload_length = 0;
    uint8_t packet_buffer[STC_STATUS_PACKET_MAX_LEN + 20] = {0};
    uint16_t index = 0;

    if (status_packet == NULL)
    {
        return STC_ISP_ERROR_PROTOCOL;
    }

    memset(status_packet, 0, sizeof(STC_StatusPacketTypeDef));

    if (STC_ISP_ReadByte(uart_instance, &byte, timeout_ms) != STC_ISP_OK)
    {
        return STC_ISP_ERROR_TIMEOUT;
    }

    if (byte == STC_PACKET_MCU)
    {
        packet_buffer[index++] = STC_PACKET_START_0;
        packet_buffer[index++] = STC_PACKET_START_1;
        packet_buffer[index++] = STC_PACKET_MCU;
    }
    else if (byte == STC_PACKET_START_0)
    {
        packet_buffer[index++] = byte;
        if (STC_ISP_ReadByte(uart_instance, &byte, timeout_ms) != STC_ISP_OK)
        {
            return STC_ISP_ERROR_TIMEOUT;
        }
        if (byte != STC_PACKET_START_1)
        {
            return STC_ISP_ERROR_FRAMING;
        }
        packet_buffer[index++] = byte;

        if (STC_ISP_ReadByte(uart_instance, &byte, timeout_ms) != STC_ISP_OK)
        {
            return STC_ISP_ERROR_TIMEOUT;
        }
        if (byte != STC_PACKET_MCU)
        {
            return STC_ISP_ERROR_FRAMING;
        }
        packet_buffer[index++] = byte;
    }
    else
    {
        return STC_ISP_ERROR_FRAMING;
    }

    if (STC_ISP_ReadByte(uart_instance, &len_high, timeout_ms) != STC_ISP_OK)
    {
        return STC_ISP_ERROR_TIMEOUT;
    }
    if (STC_ISP_ReadByte(uart_instance, &len_low, timeout_ms) != STC_ISP_OK)
    {
        return STC_ISP_ERROR_TIMEOUT;
    }
    packet_length = ((uint16_t)len_high << 8) | len_low;
    packet_buffer[index++] = len_high;
    packet_buffer[index++] = len_low;

    if ((packet_length < 3U) || (packet_length > (STC_STATUS_PACKET_MAX_LEN + 20)))
    {
        return STC_ISP_ERROR_FRAMING;
    }

    payload_length = packet_length - 3U;
    for (uint16_t i = 0; i < payload_length; i++)
    {
        if (STC_ISP_ReadByte(uart_instance, &byte, timeout_ms) != STC_ISP_OK)
        {
            return STC_ISP_ERROR_TIMEOUT;
        }
        packet_buffer[index++] = byte;
    }

    if (packet_buffer[index - 1] != STC_PACKET_END)
    {
        return STC_ISP_ERROR_FRAMING;
    }

    uint16_t checksum_pos = (uint16_t)(index - 3);
    uint16_t calc_checksum = STC_ISP_CalculateChecksum(&packet_buffer[2], (uint16_t)(checksum_pos - 2));
    uint16_t recv_checksum = ((uint16_t)packet_buffer[checksum_pos] << 8) | packet_buffer[checksum_pos + 1];

    if (calc_checksum != recv_checksum)
    {
        return STC_ISP_ERROR_FRAMING;
    }

    uint16_t payload_start = 5U;
    uint16_t payload_len = (uint16_t)(checksum_pos - payload_start);
    if (payload_len > STC_STATUS_PACKET_MAX_LEN)
    {
        payload_len = STC_STATUS_PACKET_MAX_LEN;
    }

    memcpy(status_packet->data, &packet_buffer[payload_start], payload_len);
    status_packet->length = payload_len;

    return STC_ISP_OK;
}

STC_ISP_StatusTypeDef STC_ISP_ParseStatusPacket(STC_StatusPacketTypeDef *status_packet)
{
    if ((status_packet == NULL) || (status_packet->length < 23U))
    {
        return STC_ISP_ERROR_PROTOCOL;
    }

    status_packet->mcu_magic = ((uint16_t)status_packet->data[20] << 8) | status_packet->data[21];
    if (status_packet->mcu_magic != STC8G1K08A_MAGIC)
    {
        return STC_ISP_ERROR_MAGIC_MISMATCH;
    }

    uint8_t bl_version = status_packet->data[17];
    uint8_t bl_stepping = status_packet->data[18];
    uint8_t bl_minor = 0;
    if (status_packet->length >= 23U)
    {
        bl_minor = status_packet->data[22] & 0x0F;
    }
    snprintf(status_packet->bsl_version, sizeof(status_packet->bsl_version),
             "%d.%d.%d%c",
             bl_version >> 4,
             bl_version & 0x0F,
             bl_minor,
             (char)bl_stepping);

    status_packet->mcu_clock_hz = ((uint32_t)status_packet->data[1] << 24) |
                                  ((uint32_t)status_packet->data[2] << 16) |
                                  ((uint32_t)status_packet->data[3] << 8) |
                                  (uint32_t)status_packet->data[4];
    if (status_packet->mcu_clock_hz == 0xFFFFFFFF)
    {
        status_packet->mcu_clock_hz = 0;
    }

    return STC_ISP_OK;
}

void STC_ISP_PrintStatusPacket(STC_StatusPacketTypeDef *status_packet)
{
    if (status_packet == NULL)
    {
        return;
    }

    // printf("\r\n=== STC ISP Status Packet ===\r\n");
    // printf("Length: %u bytes\r\n", status_packet->length);
    // printf("MCU Magic: 0x%04X\r\n", status_packet->mcu_magic);
    // printf("BSL Version: %s\r\n", status_packet->bsl_version);
    if (status_packet->mcu_clock_hz > 0U)
    {
        // printf("MCU Clock: %.3f MHz\r\n", status_packet->mcu_clock_hz / 1000000.0f);
    }
    else
    {
        // printf("MCU Clock: Not calibrated\r\n");
    }

    // printf("Raw Data (first 32 bytes):\r\n");
    for (uint16_t i = 0; (i < status_packet->length) && (i < 32U); i++)
    {
        // printf("%02X ", status_packet->data[i]);
        if (((i + 1U) % 16U) == 0U)
        {
            // printf("\r\n");
        }
    }
    // printf("\r\n============================\r\n");
}

STC_ISP_StatusTypeDef STC_ISP_HandshakeTest(USART_TypeDef *uart_instance)
{
    STC_ISP_StatusTypeDef status = STC_ISP_OK;
    STC_StatusPacketTypeDef status_packet;
    char lcd_buffer[21] = {0};

    STC_ISP_UpdateLCDStatus("Step 1: Config UART");
    // printf("[STC] Config UART for handshake: 2400 baud, even parity\r\n");
    STC_ISP_ReconfigureUart(uart_instance, STC_HANDSHAKE_BAUD, LL_USART_PARITY_EVEN);
    HAL_Delay(200);  // 增加等待时间，确保UART配置稳定
    STC_ISP_UpdateLCDStatus("Step 1: UART Config OK");

    // 先开始持续发送0x7F，然后在发送过程中进行冷启动
    STC_ISP_UpdateLCDStatus("Step 2: Start Sync...");
    
    // 启动同步脉冲发送（在后台持续发送）
    // 注意：这里不等待响应，只是开始发送
    
    // 在发送过程中进行冷启动
    STC_ISP_UpdateLCDStatus("Step 3: Cold Start...");
    LL_GPIO_ResetOutputPin(STC_POWER_CTRL_PORT, STC_POWER_CTRL_PIN);
    
    // 在断电期间继续发送0x7F（虽然STC芯片断电了，但保持发送状态）
    uint32_t power_off_start = HAL_GetTick();
    while ((HAL_GetTick() - power_off_start) < 3000)
    {
        // 每30ms发送一次0x7F
        static uint32_t last_send = 0;
        if ((HAL_GetTick() - last_send) >= 30)
        {
            if (LL_USART_IsActiveFlag_TXE(uart_instance))
            {
                LL_USART_TransmitData8(uart_instance, STC_SYNC_CHAR);
                while (!LL_USART_IsActiveFlag_TC(uart_instance)) {}
                last_send = HAL_GetTick();
            }
        }
        HAL_Delay(1);
    }
    
    // 上电，STC芯片开始启动
    LL_GPIO_SetOutputPin(STC_POWER_CTRL_PORT, STC_POWER_CTRL_PIN);
    STC_ISP_UpdateLCDStatus("Step 3: Power On...");
    
    // 上电后继续发送并等待响应
    STC_ISP_UpdateLCDStatus("Step 4: Send Sync...");
    // printf("[STC] Sending sync pulse (0x7F)...\r\n");
    status = STC_ISP_SendSyncPulse(uart_instance, 5000U);
    if (status != STC_ISP_OK)
    {
        // printf("[STC] Sync pulse timeout\r\n");
        LCD_SetTextColor(Red);
        LCD_SetBackColor(Black);
        LCD_DisplayStringLine(Line0, (u8 *)"STC ISP Handshake");
        LCD_DisplayStringLine(Line1, (u8 *)"Step 4: Timeout!");
        LCD_DisplayStringLine(Line2, (u8 *)"Check connection");
        return status;
    }
    // printf("[STC] Sync acknowledged by target\r\n");
    STC_ISP_UpdateLCDStatus("Step 4: Sync OK");

    STC_ISP_UpdateLCDStatus("Step 5: Read Packet...");
    // printf("[STC] Reading status packet...\r\n");
    status = STC_ISP_ReadStatusPacket(uart_instance, &status_packet, 2000U);
    if (status != STC_ISP_OK)
    {
        // printf("[STC] Failed to read status packet\r\n");
        STC_ISP_UpdateLCDStatus("Step 5: Read Failed!");
        return status;
    }
    // printf("[STC] Status packet received (%u bytes)\r\n", status_packet.length);
    snprintf(lcd_buffer, sizeof(lcd_buffer), "Step 5: Read %u bytes", status_packet.length);
    STC_ISP_UpdateLCDStatus(lcd_buffer);

    STC_ISP_UpdateLCDStatus("Step 6: Parse Data...");
    status = STC_ISP_ParseStatusPacket(&status_packet);
    if (status == STC_ISP_ERROR_MAGIC_MISMATCH)
    {
        // printf("[STC] WARNING: Magic mismatch (0x%04X)\r\n", status_packet.mcu_magic);
        STC_ISP_UpdateLCDStatus("Step 6: Magic Warn!");
    }
    else if (status != STC_ISP_OK)
    {
        // printf("[STC] Failed to parse status packet\r\n");
        STC_ISP_UpdateLCDStatus("Step 6: Parse Failed!");
        return status;
    }
    else
    {
        STC_ISP_UpdateLCDStatus("Step 6: Parse OK");
    }

    STC_ISP_PrintStatusPacket(&status_packet);
    // printf("[STC] Handshake success\r\n");

    /* Display success message with version */
    snprintf(lcd_buffer, sizeof(lcd_buffer), "SUCCESS! Ver:%s", status_packet.bsl_version);
    LCD_SetTextColor(Green);
    LCD_SetBackColor(Black);
    LCD_DisplayStringLine(Line0, (u8 *)"STC ISP Handshake");
    LCD_DisplayStringLine(Line1, (u8 *)lcd_buffer);

    /* Display additional info on line 2 */
    if (status_packet.mcu_clock_hz > 0U)
    {
        snprintf(lcd_buffer, sizeof(lcd_buffer), "Clock: %.1f MHz", status_packet.mcu_clock_hz / 1000000.0f);
        LCD_DisplayStringLine(Line2, (u8 *)lcd_buffer);
    }
    else
    {
        LCD_DisplayStringLine(Line2, (u8 *)"Clock: Not calibrated");
    }

    return STC_ISP_OK;
}
