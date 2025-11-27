/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../Service/log.h"
#include "../Service/log_uart_adapter.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint32_t adc_get[1] = {0};
// 串口接收字符缓冲区（用于LCD显示）
char uart2_rx_buffer[21] = {0}; // 20个字符 + 结束符
uint8_t uart2_rx_updated = 0;   // 标志位，表示有新数据需要更新显示
// 串口接收错误标志
uint8_t uart2_rx_error = 0;     // 错误标志：1=校验错误(PE), 2=帧错误(FE), 3=噪声错误(NE), 4=溢出错误(ORE)
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void UpdateADC_Display(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
  HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc_get, 1);
  // Initialize log service
  LCD_Init();
  LCD_Clear(Black);
  log_init();
  HAL_Delay(3000);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET); // LED OFF
  // Register UART output adapter (use printf format for better performance)
  if (log_uart_adapter_init_printf() == 0)
  {
    // Set log level
    log_set_level(LOG_LEVEL_DEBUG);
  }
  else
  {
    // If printf format registration fails, try raw data format
    if (log_uart_adapter_init() == 0)
    {
      log_set_level(LOG_LEVEL_DEBUG);
    }
  }


  /* USER CODE END 2 */
  
  // 禁用USART2接收中断，改用轮询方式
  LL_USART_DisableIT_RXNE(USART2);
  
  // STC8握手帧头: 46 B9 68
  static const uint8_t STC8_FRAME_HEADER[3] = {0x46, 0xB9, 0x68};

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // STC8握手：每30ms发送0x7F，等待BSL响应帧(46 B9 68开头)
    static uint32_t tx_count = 0;
    static uint8_t frame_detected = 0;
    static uint8_t rx_buffer[64] = {0};  // 接收缓冲区
    static uint8_t rx_index = 0;
    char lcd_buffer[21] = {0};
    
    // 清除可能存在的错误标志
    if (LL_USART_IsActiveFlag_ORE(USART2)) LL_USART_ClearFlag_ORE(USART2);
    if (LL_USART_IsActiveFlag_FE(USART2)) LL_USART_ClearFlag_FE(USART2);
    if (LL_USART_IsActiveFlag_NE(USART2)) LL_USART_ClearFlag_NE(USART2);
    if (LL_USART_IsActiveFlag_PE(USART2)) LL_USART_ClearFlag_PE(USART2);
    
    // 发送0x7F握手字节
    while (!LL_USART_IsActiveFlag_TXE(USART2))
    {
      // 等待发送寄存器为空
    }
    LL_USART_TransmitData8(USART2, 0x7F);
    
    // 等待发送完成
    while (!LL_USART_IsActiveFlag_TC(USART2))
    {
      // 等待传输完成
    }
    
    tx_count++;
    
    // 轮询接收（在30ms内尽量多接收数据）
    uint32_t start_tick = HAL_GetTick();
    while ((HAL_GetTick() - start_tick) < 30)  // 30ms超时
    {
      // 检查错误标志
      if (LL_USART_IsActiveFlag_ORE(USART2))
      {
        LL_USART_ClearFlag_ORE(USART2);
        LL_USART_ReceiveData8(USART2);  // 清除数据
      }
      if (LL_USART_IsActiveFlag_FE(USART2))
      {
        LL_USART_ClearFlag_FE(USART2);
      }
      if (LL_USART_IsActiveFlag_PE(USART2))
      {
        LL_USART_ClearFlag_PE(USART2);
      }
      
      // 检查是否有数据接收
      if (LL_USART_IsActiveFlag_RXNE(USART2))
      {
        uint8_t data = LL_USART_ReceiveData8(USART2);
        
        // 存入缓冲区
        if (rx_index < sizeof(rx_buffer))
        {
          rx_buffer[rx_index++] = data;
        }
        
        // 检查是否收到帧头 46 B9 68
        if (rx_index >= 3)
        {
          // 在缓冲区中搜索帧头
          for (uint8_t i = 0; i <= rx_index - 3; i++)
          {
            if (rx_buffer[i] == STC8_FRAME_HEADER[0] &&
                rx_buffer[i+1] == STC8_FRAME_HEADER[1] &&
                rx_buffer[i+2] == STC8_FRAME_HEADER[2])
            {
              frame_detected = 1;
              break;
            }
          }
        }
      }
    }
    
    // 显示状态到LCD
    LCD_SetTextColor(White);
    LCD_SetBackColor(Black);
    
    LCD_DisplayStringLine(Line0, (u8 *)"STC8 ISP Handshake");
    LCD_DisplayStringLine(Line1, (u8 *)"2400bps EVEN parity");
    
    snprintf(lcd_buffer, sizeof(lcd_buffer), "TX 0x7F: %lu", tx_count);
    LCD_DisplayStringLine(Line2, (u8 *)lcd_buffer);
    
    // 显示接收状态
    if (frame_detected)
    {
      LCD_SetTextColor(Green);
      LCD_DisplayStringLine(Line3, (u8 *)"Frame: 46 B9 68 OK!");
      
      // 显示接收到的前几个字节
      LCD_SetTextColor(Cyan);
      snprintf(lcd_buffer, sizeof(lcd_buffer), "RX:%02X %02X %02X %02X %02X",
               rx_buffer[0], rx_buffer[1], rx_buffer[2],
               rx_index > 3 ? rx_buffer[3] : 0,
               rx_index > 4 ? rx_buffer[4] : 0);
      LCD_DisplayStringLine(Line4, (u8 *)lcd_buffer);
    }
    else if (rx_index > 0)
    {
      LCD_SetTextColor(Yellow);
      snprintf(lcd_buffer, sizeof(lcd_buffer), "RX %d bytes", rx_index);
      LCD_DisplayStringLine(Line3, (u8 *)lcd_buffer);
      
      // 显示接收到的数据（最多显示5字节）
      snprintf(lcd_buffer, sizeof(lcd_buffer), "Data:%02X %02X %02X %02X %02X",
               rx_buffer[0],
               rx_index > 1 ? rx_buffer[1] : 0,
               rx_index > 2 ? rx_buffer[2] : 0,
               rx_index > 3 ? rx_buffer[3] : 0,
               rx_index > 4 ? rx_buffer[4] : 0);
      LCD_DisplayStringLine(Line4, (u8 *)lcd_buffer);
    }
    else
    {
      LCD_SetTextColor(Red);
      LCD_DisplayStringLine(Line3, (u8 *)"Waiting STC8...");
      LCD_DisplayStringLine(Line4, (u8 *)"Power on STC8 now!");
    }
    
    // 如果检测到帧头，不再清空缓冲区，保持状态
    if (!frame_detected)
    {
      // 清空接收缓冲区，准备下一轮
      rx_index = 0;
      memset(rx_buffer, 0, sizeof(rx_buffer));
    }
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV2;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief 更新LCD显示ADC值
 * @note  显示ADC原始值和计算后的电压值
 */
static void UpdateADC_Display(void)
{
  char lcd_buffer[21] = {0};
  // 从32位变量中读取16位ADC值（低16位有效）
  uint16_t adc_raw = (uint16_t)(adc_get[0] & 0xFFFF);

  // ADC是12位的，最大值4095
  // 假设参考电压为3.3V（根据实际硬件调整）
  float voltage = (float)adc_raw * 3.3f / 4095.0f;

  // 设置LCD颜色
  LCD_SetTextColor(White);
  LCD_SetBackColor(Black);

  // 第一行：显示标题
  LCD_DisplayStringLine(Line0, (u8 *)"ADC Monitor PB15");

  // 第二行：显示原始值
  snprintf(lcd_buffer, sizeof(lcd_buffer), "Raw: %-5u", adc_raw);
  LCD_DisplayStringLine(Line1, (u8 *)lcd_buffer);

  // 第三行：显示电压值
  snprintf(lcd_buffer, sizeof(lcd_buffer), "Volt: %.3fV", voltage);
  LCD_DisplayStringLine(Line2, (u8 *)lcd_buffer);
}


/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
