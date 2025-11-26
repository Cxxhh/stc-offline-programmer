/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    stm32g4xx_it.c
 * @brief   Interrupt Service Routines.
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
#include "stm32g4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32g4xx_ll_dma.h"
#include "stm32g4xx_ll_usart.h"
#include "../BSP/bsp_spi.h"
#include <string.h>
/* USER CODE END Includes */

/* External variables ---------------------------------------------------------*/
/* USER CODE BEGIN EV */
extern char uart2_rx_buffer[21];
extern uint8_t uart2_rx_updated;
extern uint8_t uart2_rx_error;  // 错误标志：1=校验错误(PE), 2=帧错误(FE), 3=噪声错误(NE), 4=溢出错误(ORE)
/* USER CODE END EV */
/* USER CODE BEGIN EV */
extern char uart2_rx_buffer[21];
extern uint8_t uart2_rx_updated;
/* USER CODE END EV */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
// 串口数据处理函数声明
void HandleUartIT(uint8_t data);
void HandleUart2IT(uint8_t data);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
 * @brief This function handles Non maskable interrupt.
 */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
 * @brief This function handles Hard fault interrupt.
 */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
 * @brief This function handles Memory management fault.
 */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
 * @brief This function handles Prefetch fault, memory access fault.
 */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
 * @brief This function handles Undefined instruction or illegal state.
 */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
 * @brief This function handles System service call via SWI instruction.
 */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
 * @brief This function handles Debug monitor.
 */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
 * @brief This function handles Pendable request for system service.
 */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32G4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32g4xx.s).                    */
/******************************************************************************/

/**
 * @brief This function handles DMA1 channel1 global interrupt.
 */
void DMA1_Channel1_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel1_IRQn 0 */
  // SPI1 RX DMA中断处理
  if (LL_DMA_IsActiveFlag_TC1(DMA1))
  {
    LL_DMA_ClearFlag_TC1(DMA1);
    bsp_spi_dma_rx_complete_callback();
  }

  if (LL_DMA_IsActiveFlag_TE1(DMA1))
  {
    LL_DMA_ClearFlag_TE1(DMA1);
    bsp_spi_dma_error_callback();
  }
  /* USER CODE END DMA1_Channel1_IRQn 0 */
  /* USER CODE BEGIN DMA1_Channel1_IRQn 1 */

  /* USER CODE END DMA1_Channel1_IRQn 1 */
}

/**
 * @brief This function handles DMA1 channel2 global interrupt.
 */
void DMA1_Channel2_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Channel2_IRQn 0 */
  // SPI1 TX DMA中断处理
  if (LL_DMA_IsActiveFlag_TC2(DMA1))
  {
    LL_DMA_ClearFlag_TC2(DMA1);
    bsp_spi_dma_tx_complete_callback();
  }

  if (LL_DMA_IsActiveFlag_TE2(DMA1))
  {
    LL_DMA_ClearFlag_TE2(DMA1);
    bsp_spi_dma_error_callback();
  }
  /* USER CODE END DMA1_Channel2_IRQn 0 */
  /* USER CODE BEGIN DMA1_Channel2_IRQn 1 */

  /* USER CODE END DMA1_Channel2_IRQn 1 */
}

/**
 * @brief This function handles SPI1 global interrupt.
 */
void SPI1_IRQHandler(void)
{
  /* USER CODE BEGIN SPI1_IRQn 0 */

  /* USER CODE END SPI1_IRQn 0 */
  /* USER CODE BEGIN SPI1_IRQn 1 */

  /* USER CODE END SPI1_IRQn 1 */
}

/**
 * @brief This function handles USART1 global interrupt / USART1 wake-up interrupt through EXTI line 25.
 */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */
  uint8_t data;
  
  // 检查并清除错误标志（防止FF干扰）
  if (LL_USART_IsActiveFlag_ORE(USART1))
  {
    LL_USART_ClearFlag_ORE(USART1);
    // 读取DR寄存器以清除ORE标志
    (void)LL_USART_ReceiveData8(USART1);
  }
  if (LL_USART_IsActiveFlag_FE(USART1))
  {
    LL_USART_ClearFlag_FE(USART1);
    // 读取DR寄存器以清除FE标志
    (void)LL_USART_ReceiveData8(USART1);
  }
  if (LL_USART_IsActiveFlag_NE(USART1))
  {
    LL_USART_ClearFlag_NE(USART1);
    // 读取DR寄存器以清除NE标志
    (void)LL_USART_ReceiveData8(USART1);
  }
  if (LL_USART_IsActiveFlag_PE(USART1))
  {
    LL_USART_ClearFlag_PE(USART1);
    // 读取DR寄存器以清除PE标志
    (void)LL_USART_ReceiveData8(USART1);
  }
  
  // 正常接收数据
  if (LL_USART_IsEnabledIT_RXNE(USART1) && LL_USART_IsActiveFlag_RXNE(USART1))
  {
    data = LL_USART_ReceiveData8(USART1);
    // 注意：读取DR寄存器会自动清除RXNE标志，无需手动清除
    HandleUartIT(data);
  }
  /* USER CODE END USART1_IRQn 0 */
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
 * @brief This function handles USART2 global interrupt / USART2 wake-up interrupt through EXTI line 26.
 */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */
  uint8_t data;
  
  // 首先检查并清除错误标志（防止0xFF干扰）
  // 这些错误通常是由于校验位不匹配、帧错误等引起的
  if (LL_USART_IsActiveFlag_ORE(USART2))
  {
    LL_USART_ClearFlag_ORE(USART2);
    // 读取DR寄存器以清除ORE标志
    uint8_t error_data = LL_USART_ReceiveData8(USART2);
    // 记录错误类型
    uart2_rx_error = 4; // ORE错误
    strncpy(uart2_rx_buffer, "ERR: Overrun", 20);
    uart2_rx_buffer[20] = '\0';
    uart2_rx_updated = 1;
  }
  if (LL_USART_IsActiveFlag_FE(USART2))
  {
    LL_USART_ClearFlag_FE(USART2);
    // 读取DR寄存器以清除FE标志（帧错误）
    uint8_t error_data = LL_USART_ReceiveData8(USART2);
    // 记录错误类型
    uart2_rx_error = 2; // FE错误
    strncpy(uart2_rx_buffer, "ERR: Frame Error", 20);
    uart2_rx_buffer[20] = '\0';
    uart2_rx_updated = 1;
  }
  if (LL_USART_IsActiveFlag_NE(USART2))
  {
    LL_USART_ClearFlag_NE(USART2);
    // 读取DR寄存器以清除NE标志（噪声错误）
    uint8_t error_data = LL_USART_ReceiveData8(USART2);
    // 记录错误类型
    uart2_rx_error = 3; // NE错误
    strncpy(uart2_rx_buffer, "ERR: Noise Error", 20);
    uart2_rx_buffer[20] = '\0';
    uart2_rx_updated = 1;
  }
  if (LL_USART_IsActiveFlag_PE(USART2))
  {
    LL_USART_ClearFlag_PE(USART2);
    // 读取DR寄存器以清除PE标志（校验错误）
    // 校验错误时读取的数据可能是0xFF
    uint8_t error_data = LL_USART_ReceiveData8(USART2);
    // 校验错误：说明发送端和接收端的校验位不匹配
    // 记录错误类型
    uart2_rx_error = 1; // PE错误
    // 显示错误信息
    strncpy(uart2_rx_buffer, "ERR: Parity Error", 20);
    uart2_rx_buffer[20] = '\0';
    uart2_rx_updated = 1;
  }
  
  // 正常接收数据（只有在没有错误标志时才处理）
  if (LL_USART_IsEnabledIT_RXNE(USART2) && LL_USART_IsActiveFlag_RXNE(USART2))
  {
    // 再次检查是否有错误标志（读取数据前）
    if (!LL_USART_IsActiveFlag_PE(USART2) && 
        !LL_USART_IsActiveFlag_FE(USART2) && 
        !LL_USART_IsActiveFlag_NE(USART2))
    {
      data = LL_USART_ReceiveData8(USART2);
      // 注意：读取DR寄存器会自动清除RXNE标志，无需手动清除
      // 清除错误标志（接收到正常数据）
      uart2_rx_error = 0;
      HandleUart2IT(data);
    }
    else
    {
      // 有错误标志，先清除错误标志
      if (LL_USART_IsActiveFlag_PE(USART2))
      {
        LL_USART_ClearFlag_PE(USART2);
      }
      if (LL_USART_IsActiveFlag_FE(USART2))
      {
        LL_USART_ClearFlag_FE(USART2);
        uart2_rx_error = 2; // FE错误
        strncpy(uart2_rx_buffer, "ERR: Frame Error", 20);
        uart2_rx_buffer[20] = '\0';
        uart2_rx_updated = 1;
      }
      if (LL_USART_IsActiveFlag_NE(USART2))
      {
        LL_USART_ClearFlag_NE(USART2);
        uart2_rx_error = 3; // NE错误
        strncpy(uart2_rx_buffer, "ERR: Noise Error", 20);
        uart2_rx_buffer[20] = '\0';
        uart2_rx_updated = 1;
      }
      // 读取并丢弃错误数据
      uint8_t error_data = LL_USART_ReceiveData8(USART2);
      // 如果是0xFF，说明可能是校验错误导致的
      if (error_data == 0xFF)
      {
        uart2_rx_error = 1; // 可能是PE错误
        strncpy(uart2_rx_buffer, "RX: 0xFF (Error)", 20);
        uart2_rx_buffer[20] = '\0';
        uart2_rx_updated = 1;
      }
    }
  }
  /* USER CODE END USART2_IRQn 0 */
  /* USER CODE BEGIN USART2_IRQn 1 */

  /* USER CODE END USART2_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
 * @brief 处理串口接收到的数据并回发
 * @param data 接收到的数据
 */
void HandleUartIT(uint8_t data)
{
  // 等待发送寄存器为空
  while (!LL_USART_IsActiveFlag_TXE(USART1))
  {
    // 等待发送就绪
  }

  // 将接收到的数据发送回去
  LL_USART_TransmitData8(USART1, data);

  // 等待发送完成
  while (!LL_USART_IsActiveFlag_TC(USART1))
  {
    // 等待传输完成
  }
}

/**
 * @brief 处理USART2接收到的数据并通过USART1发送
 * @param data 接收到的数据
 */
void HandleUart2IT(uint8_t data)
{
  // 将接收到的字符存入缓冲区（用于LCD显示）
  static uint8_t buffer_index = 0;
  
  // 处理接收到的字符
  if (data >= 32 && data <= 126)
  {
    // 可打印字符，直接存入缓冲区
    if (buffer_index < 20)
    {
      uart2_rx_buffer[buffer_index] = (char)data;
      buffer_index++;
      uart2_rx_buffer[buffer_index] = '\0'; // 确保字符串结束
    }
    else
    {
      // 缓冲区已满，向左移动（FIFO），保持最新20个字符
      for (uint8_t i = 0; i < 19; i++)
      {
        uart2_rx_buffer[i] = uart2_rx_buffer[i + 1];
      }
      uart2_rx_buffer[19] = (char)data;
      uart2_rx_buffer[20] = '\0';
      buffer_index = 20; // 保持索引为20，继续使用FIFO模式
    }
    uart2_rx_updated = 1; // 标记有新数据需要更新显示
  }
  else
  {
    // 非可打印字符（包括0x7F），以十六进制形式显示
    // 格式：RX: 0xXX
    uart2_rx_buffer[0] = 'R';
    uart2_rx_buffer[1] = 'X';
    uart2_rx_buffer[2] = ':';
    uart2_rx_buffer[3] = ' ';
    uart2_rx_buffer[4] = '0';
    uart2_rx_buffer[5] = 'x';
    // 高4位
    uint8_t high_nibble = (data >> 4) & 0x0F;
    uart2_rx_buffer[6] = (high_nibble < 10) ? ('0' + high_nibble) : ('A' + high_nibble - 10);
    // 低4位
    uint8_t low_nibble = data & 0x0F;
    uart2_rx_buffer[7] = (low_nibble < 10) ? ('0' + low_nibble) : ('A' + low_nibble - 10);
    uart2_rx_buffer[8] = '\0';
    buffer_index = 0; // 重置索引，下次从新字符开始
    uart2_rx_updated = 1; // 标记有新数据需要更新显示
  }
  
  // 等待USART1发送寄存器为空
  while (!LL_USART_IsActiveFlag_TXE(USART1))
  {
    // 等待发送就绪
  }

  // 将USART2接收到的数据通过USART1发送
  LL_USART_TransmitData8(USART1, data);

  // 等待发送完成
  while (!LL_USART_IsActiveFlag_TC(USART1))
  {
    // 等待传输完成
  }
}

/* USER CODE END 1 */
