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
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "../BSP/bsp_sdcard.h"
#include "../BSP/bsp_sdcard_debug.h"
#include "../FatFS/src/ff.h"
#include <string.h>
#include <stdio.h>
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
static FATFS g_fs;           // FatFS文件系统对象
static FIL g_file;           // 文件对象
static char g_buffer[512];   // 缓冲区
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  /* USER CODE BEGIN 2 */
	
	LCD_Init();
	
	// SD卡初始化测试
	LCD_Clear(Blue);
	LCD_SetBackColor(Blue);
	LCD_SetTextColor(White);
	LCD_DisplayStringLine(Line0, (unsigned char *)"  SD Card Test     ");
	LCD_DisplayStringLine(Line1, (unsigned char *)"Initializing...   ");
	
	HAL_Delay(100);  // 等待LCD显示
	
	// 初始化SD卡
	bsp_sdcard_result_t sd_result = bsp_sdcard_init();
	
	if (sd_result == BSP_SDCARD_OK) {
		LCD_DisplayStringLine(Line1, (unsigned char *)"SD Init: OK       ");
		
		// 获取SD卡信息
		bsp_sdcard_info_t sd_info;
		bsp_sdcard_detect(&sd_info);
		
		// 显示SD卡类型
		if (sd_info.type == BSP_SDCARD_TYPE_V2HC) {
			LCD_DisplayStringLine(Line2, (unsigned char *)"Type: SDHC        ");
		} else if (sd_info.type == BSP_SDCARD_TYPE_V2) {
			LCD_DisplayStringLine(Line2, (unsigned char *)"Type: SD V2       ");
		} else if (sd_info.type == BSP_SDCARD_TYPE_V1) {
			LCD_DisplayStringLine(Line2, (unsigned char *)"Type: SD V1       ");
		}
		
		// 挂载FatFS文件系统
		FRESULT res = f_mount(&g_fs, "0:", 1);
		if (res == FR_OK) {
			LCD_DisplayStringLine(Line3, (unsigned char *)"FatFS: Mounted    ");
			
			// 测试写入文件
			res = f_open(&g_file, "0:test.txt", FA_CREATE_ALWAYS | FA_WRITE);
			if (res == FR_OK) {
				UINT bw;
				const char* test_str = "Hello from STM32G4!\r\nSD Card works!\r\n";
				f_write(&g_file, test_str, strlen(test_str), &bw);
				f_close(&g_file);
				LCD_DisplayStringLine(Line4, (unsigned char *)"Write: OK         ");
			} else {
				LCD_DisplayStringLine(Line4, (unsigned char *)"Write: Failed     ");
			}
			
			// 测试读取文件
			res = f_open(&g_file, "0:test.txt", FA_READ);
			if (res == FR_OK) {
				UINT br;
				f_read(&g_file, g_buffer, sizeof(g_buffer), &br);
				f_close(&g_file);
				LCD_DisplayStringLine(Line5, (unsigned char *)"Read: OK          ");
			} else {
				LCD_DisplayStringLine(Line5, (unsigned char *)"Read: Failed      ");
			}
		} else {
			LCD_DisplayStringLine(Line3, (unsigned char *)"FatFS: Mount Fail ");
		}
	} else {
		LCD_DisplayStringLine(Line1, (unsigned char *)"SD Init: Failed   ");
		
		// 显示诊断信息
		char diag_buf[512];
		bsp_sdcard_diagnose(diag_buf, sizeof(diag_buf));
		
		// 在LCD上显示关键诊断信息（简化版）
		LCD_DisplayStringLine(Line2, (unsigned char *)"Running Diag...   ");
		HAL_Delay(1000);
		
		// 这里可以通过UART输出完整诊断信息
		// printf("%s", diag_buf);  // 如果有串口重定向
	}
	
	HAL_Delay(3000);
	
	// 原来的LCD演示代码
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	
	LCD_Clear(Blue);
	LCD_SetBackColor(Blue);
	LCD_SetTextColor(White);
	

	LCD_DrawLine(120,0,320,Horizontal);
	LCD_DrawLine(0,160,240,Vertical);
	HAL_Delay(1000);
	LCD_Clear(Blue);

	LCD_DrawRect(70,210,100,100);
	HAL_Delay(1000);
	LCD_Clear(Blue);

	LCD_DrawCircle(120,160,50);
	HAL_Delay(1000);

	LCD_Clear(Blue);
	LCD_DisplayStringLine(Line4 ,(unsigned char *)"    Hello,world.   ");
	HAL_Delay(1000);

	LCD_SetBackColor(White);
	LCD_DisplayStringLine(Line0,(unsigned char *)"                    ");	
	LCD_SetBackColor(Black);
	LCD_DisplayStringLine(Line1,(unsigned char *)"                    ");	
	LCD_SetBackColor(Grey);
	LCD_DisplayStringLine(Line2,(unsigned char *)"                    ");
	LCD_SetBackColor(Blue);
	LCD_DisplayStringLine(Line3,(unsigned char *)"                    ");
	LCD_SetBackColor(Blue2);
	LCD_DisplayStringLine(Line4,(unsigned char *)"                    ");
	LCD_SetBackColor(Red);						
	LCD_DisplayStringLine(Line5,(unsigned char *)"                    ");
	LCD_SetBackColor(Magenta);	
	LCD_DisplayStringLine(Line6,(unsigned char *)"                    ");
	LCD_SetBackColor(Green);	
	LCD_DisplayStringLine(Line7,(unsigned char *)"                    ");	
	LCD_SetBackColor(Cyan);	
	LCD_DisplayStringLine(Line8,(unsigned char *)"                    ");
	LCD_SetBackColor(Yellow);		
	LCD_DisplayStringLine(Line9,(unsigned char *)"                    ");	
	
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
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
