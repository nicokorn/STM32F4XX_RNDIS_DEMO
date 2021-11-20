// ****************************************************************************
/// \file      led.c
///
/// \brief     LED C Source File
///
/// \details   Module to control the led which is connected
///            directly to a gpio.
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

// Include ********************************************************************
#include <stdbool.h>
#include "cmsis_os.h"
#include "led.h"

// Private define *************************************************************
#define LED_PIN   GPIO_PIN_13
#define LED_BANK  GPIOC

// Private types     **********************************************************

// Private variables **********************************************************
TIM_HandleTypeDef          htim2;
static TIM_OC_InitTypeDef  TIM_OC1Struct;
static uint8_t             duty = 20;
const osThreadAttr_t ledTask_attributes = {
  .name = "LED-task",
  .stack_size = 2 * configMINIMAL_STACK_SIZE * 4,
  .priority = (osPriority_t) osPriorityLow,
};
static bool pulse = false;

// Private function prototypes ************************************************
static void pwm_init ( void );
static void ledTask  ( void *pvParameters );

// Functions ******************************************************************
// ----------------------------------------------------------------------------
/// \brief     Initialisation of the led.
///
/// \param     none
///
/// \return    none
void led_init( void ){
   // set GPIO preipheral clock
   __HAL_RCC_GPIOC_CLK_ENABLE();
   
   // panelcontroller status led
   GPIO_InitTypeDef GPIO_Init_Struct;
   GPIO_Init_Struct.Pin    = LED_PIN;
   GPIO_Init_Struct.Pull   = GPIO_NOPULL;
   GPIO_Init_Struct.Mode   = GPIO_MODE_OUTPUT_PP;
   GPIO_Init_Struct.Speed  = GPIO_SPEED_FREQ_LOW;
   HAL_GPIO_Init(LED_BANK, &GPIO_Init_Struct);
   
   led_reset();
   
   pwm_init();
   
   // start the status monitoring task
   osThreadNew( ledTask, NULL, &ledTask_attributes );
}

// ----------------------------------------------------------------------------
/// \brief     Initialisation of the "PWM".
///
/// \param     none
///
/// \return    none
static void pwm_init( void )
{
   uint16_t PrescalerValue;
   
   // TIM2 Periph clock enable
   __HAL_RCC_TIM2_CLK_ENABLE();
   
   // set prescaler to get a 2 kHz clock signal
   PrescalerValue = (uint16_t) (SystemCoreClock / 2000) - 1;
   
   // Time base configuration
   htim2.Instance                 = TIM2;
   htim2.Init.Period              = 40;                // set the period to get 40 to get a 100 hz timer
   htim2.Init.Prescaler           = PrescalerValue;
   htim2.Init.ClockDivision       = 0;
   htim2.Init.CounterMode         = TIM_COUNTERMODE_UP;
   HAL_TIM_OC_Init(&htim2);

   // Timing Mode configuration: Capture Compare 1
   TIM_OC1Struct.OCMode                 = TIM_OCMODE_TIMING;
   TIM_OC1Struct.OCPolarity             = TIM_OCPOLARITY_HIGH;
   TIM_OC1Struct.Pulse                  = duty;
   
   // Configure the channel
   HAL_TIM_OC_ConfigChannel(&htim2, &TIM_OC1Struct, TIM_CHANNEL_1);
   
   // configure TIM2 interrupt
   HAL_NVIC_SetPriority( TIM2_IRQn, 0, 7 );
   HAL_NVIC_EnableIRQ( TIM2_IRQn );
   
   HAL_TIM_Base_Start_IT( &htim2 );
   HAL_TIM_OC_Start_IT( &htim2, TIM_CHANNEL_1 );
}

// ----------------------------------------------------------------------------
/// \brief     Output Compare callback in non blocking mode 
///
/// \param     [in]  TIM_HandleTypeDef *htim
///
/// \return    none
void HAL_TIM_OC_DelayElapsedCallback( TIM_HandleTypeDef *htim )
{
   if(htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
   {
      led_reset();
   }
}

// ----------------------------------------------------------------------------
/// \brief     DeInitialision of the led.
///
/// \param     none
///
/// \return    none
void led_deinit( void )
{
   __HAL_RCC_GPIOC_CLK_DISABLE();
   HAL_GPIO_DeInit(LED_BANK, LED_PIN);
}

// ----------------------------------------------------------------------------
/// \brief     Toggle led.
///
/// \param     none
///
/// \return    none
void led_toggle( void )
{
   HAL_GPIO_TogglePin(LED_BANK, LED_PIN);
}

// ----------------------------------------------------------------------------
/// \brief     Set led.
///
/// \param     none
///
/// \return    none
void led_set( void )
{
   HAL_GPIO_WritePin(LED_BANK, LED_PIN, GPIO_PIN_RESET);
}

// ----------------------------------------------------------------------------
/// \brief     Set duty cycle.
///
/// \param     none
///
/// \return    none
void led_setDuty( uint8_t dutyParam )
{
   duty = dutyParam;
   TIM_OC1Struct.Pulse = duty;
   HAL_TIM_OC_ConfigChannel(&htim2, &TIM_OC1Struct, TIM_CHANNEL_1);
}

// ----------------------------------------------------------------------------
/// \brief     Get duty cycle.
///
/// \param     none
///
/// \return    none
uint8_t led_getDuty( void )
{
   if( pulse != true )
   {
      return duty;
   }
   return 20;
}


// ----------------------------------------------------------------------------
/// \brief     Reset status led.
///
/// \param     none
///
/// \return    none
void led_reset( void )
{
   HAL_GPIO_WritePin(LED_BANK, LED_PIN, GPIO_PIN_SET);
}

// ----------------------------------------------------------------------------
/// \brief     Test of the led. 
///
/// \param     none
///
/// \return    none
void led_test( void )
{
   HAL_GPIO_WritePin(LED_BANK, LED_PIN, GPIO_PIN_SET);
   HAL_Delay(200);
   HAL_GPIO_WritePin(LED_BANK, LED_PIN, GPIO_PIN_RESET);
   HAL_Delay(200);
}

//-----------------------------------------------------------------------------
/// \brief     Led pulse task.
///
/// \param     [in]  pvParameters
///
/// \return    none
static void ledTask( void *pvParameters )
{
   bool up = true;

   while (1)
   {
      if( pulse != true )
      {
         vTaskDelay(200);
         continue;
      }
      
      if( up != false && duty < 40 )
      {
         duty++;
         led_setDuty(duty);
         if( duty == 40 )
         {
            up = false;
         }
      }
      else if(  up != true && duty > 1  )
      {
         duty--;
         led_setDuty(duty);
         if( duty == 1 )
         {
            up = true;
         }
      }

      vTaskDelay(10);
   }
}

//-----------------------------------------------------------------------------
/// \brief     Set led led into pulse mode.
///
/// \param     none
///
/// \return    none
void led_setPulse( void )
{
   pulse = true;
}

//-----------------------------------------------------------------------------
/// \brief     Set led led into dim mode.
///
/// \param     none
///
/// \return    none
void led_setDim( void )
{
   pulse = false;
}