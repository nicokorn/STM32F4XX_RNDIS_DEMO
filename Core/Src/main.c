// ****************************************************************************
/// \file      main.c
///
/// \brief     main C source file
///
/// \details   Main c file as the applications entry point.
///
/// \author    Nico Korn
///
/// \version   0.3.0.2
///
/// \date      14112021
/// 
/// \copyright Copyright (C) 2021 by "Nico Korn". nico13@hispeed.ch
///            
///            Permission is hereby granted, free of charge, to any person 
///            obtaining a copy of this software and associated documentation 
///            files (the "Software"), to deal in the Software without 
///            restriction, including without limitation the rights to use, 
///            copy, modify, merge, publish, distribute, sublicense, and/or sell
///            copies of the Software, and to permit persons to whom the 
///            Software is furnished to do so, subject to the following 
///            conditions:
///            
///            The above copyright notice and this permission notice shall be 
///            included in all copies or substantial portions of the Software.
///            
///            THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
///            EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES 
///            OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
///            NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
///            HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
///            WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
///            FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR 
///            OTHER DEALINGS IN THE SOFTWARE.
///
/// \pre       
///
/// \bug       
///
/// \warning   
///
/// \todo      
///
// ****************************************************************************
     
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "usb_device.h"
#include "usb_device.h"
#include "queuex.h"
#include "tcpip.h"
#include "led.h"
#include "monitor.h"
#include "dhcpserver.h"

// Private typedef *************************************************************

// Private define *************************************************************

// Private variables **********************************************************
queue_handle_t tcpQueue;
queue_handle_t usbQueue;
   
// Private function prototypes ************************************************
static void systemClock_Config   ( void );
static void init_btn             ( void );

// Private functions **********************************************************

// ----------------------------------------------------------------------------
/// \brief     Main function, app entry point.
///
/// \param     none
///
/// \return    none
int main( void )
{
   // Reset of all peripherals, Initializes the Flash interface and the Systick.
   HAL_Init();
   
   // Configure the system clock.
   systemClock_Config();
   
   // Init peripherals
   monitor_init();
   led_init();
   tcpip_init();
   usb_init();
   init_btn();
   
   // Set the queue on the tcpip stack io
   tcpQueue.messageDirection  = TCP_TO_USB;
   tcpQueue.output            = usb_output;
   queue_init(&tcpQueue);
   
   // Set the queue on the usb io
   usbQueue.messageDirection   = USB_TO_TCP;
   usbQueue.output             = tcpip_output;  
   queue_init(&usbQueue);
   
   // Init scheduler
   osKernelInitialize();
   
   // Start scheduler
   osKernelStart();
   
   // Infinite loop
   while (1)
   {
   }
}

// ----------------------------------------------------------------------------
/// \brief     Called by the task.c freertos module. Calling origin is the idle
///            task.
///
/// \param     none
///
/// \return    none
void vApplicationIdleHook( void )
{
   queue_manager( &tcpQueue );
   queue_manager( &usbQueue );
}

// ----------------------------------------------------------------------------
/// \brief     Called by the task.c freertos module. Calling origin is the idle
///            task.
///
/// \param     none
///
/// \return    none
static void init_btn( void )
{
   // Enable GPIOA Clock
   __HAL_RCC_GPIOA_CLK_ENABLE();
  
   // Configure PA0 as input for getting the btn status.
   GPIO_InitTypeDef  GPIO_InitStruct;
   GPIO_InitStruct.Pin     = GPIO_PIN_0;
   GPIO_InitStruct.Mode    = GPIO_MODE_INPUT;
   GPIO_InitStruct.Pull    = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct); 
}

// ----------------------------------------------------------------------------
/// \brief     System clock configuration.
///
/// \param     none
///
/// \return    none
void systemClock_Config( void )
{
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
   
   /** Configure the main internal regulator output voltage
   */
   __HAL_RCC_PWR_CLK_ENABLE();
   __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
   /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
   RCC_OscInitStruct.HSEState = RCC_HSE_ON;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
   RCC_OscInitStruct.PLL.PLLM = 25;
   RCC_OscInitStruct.PLL.PLLN = 192;
   RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
   RCC_OscInitStruct.PLL.PLLQ = 4;
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
   RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
   
   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
   {
      Error_Handler();
   }
}

// ----------------------------------------------------------------------------
/// \brief     Period elapsed callback in non blocking mode. This function is 
///            called  when TIM1 interrupt took place, inside 
///            HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to 
///            incrementa global variable "uwTick" used as application time 
///            base.
///
/// \param     [in]  TIM_HandleTypeDef *htim
///
/// \return    none
void HAL_TIM_PeriodElapsedCallback( TIM_HandleTypeDef *htim )
{
   if (htim->Instance == TIM1)
   {
      HAL_IncTick();
   }
   if (htim->Instance == TIM2)
   {
      led_set();
   }
}

// ----------------------------------------------------------------------------
/// \brief     Error handler.
///
/// \param     [in]  TIM_HandleTypeDef *htim
///
/// \return    none
void Error_Handler(void)
{
   // User can add his own implementation to report the HAL error return state
   __disable_irq();
   while (1)
   {
   }
}

#ifdef  USE_FULL_ASSERT
// ----------------------------------------------------------------------------
/// \brief     Reports the name of the source file and the source line number
///            where the assert_param error has occurred.
///
/// \param     [in]  uint8_t *file
/// \param     [in]  uint32_t line
///
/// \return    none
void assert_failed( uint8_t *file, uint32_t line )
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
}
#endif /* USE_FULL_ASSERT */

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/