// ****************************************************************************
/// \file      monitor.c
///
/// \brief     monitor c source file
///
/// \details   This module contents actions to show the physical status of the
///			   board like temperature and input voltage.
///
/// \author    Nico Korn
///
/// \version   0.3.0.1
///
/// \date      08112021
/// 
/// \copyright Copyright 2021 Reichle & De-Massari AG
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
#include "cmsis_os.h"
#include "monitor.h"

// Private define *************************************************************
#define TEMP_SENSOR_AVG_SLOPE_MV_PER_CELSIUS                        2.5f
#define TEMP_SENSOR_VOLTAGE_MV_AT_25                                760.0f
#define ADC_REFERENCE_VOLTAGE_V                                     3.5f
#define ADC_MAX_OUTPUT_VALUE                                        4095.0f
#define TS30                                                        (int32_t)(*((uint16_t*) 0x1FFF7A2C ))
#define TS110                                                       (int32_t)(*((uint16_t*) 0x1FFF7A2E ))
#define VREF_CAL                                                    (int32_t)(*((uint16_t*) 0x1FFF7A2A ))

// Private types     **********************************************************

// Private variables **********************************************************
static float temperature = 22.0;
static float voltage = 3.0;
const osThreadAttr_t monitorTask_attributes = {
  .name = "MONITOR-task",
  .stack_size = configMINIMAL_STACK_SIZE * 4,
  .priority = (osPriority_t) osPriorityLow,
};

// Global variables ***********************************************************
ADC_HandleTypeDef ADC_Handle;
ADC_HandleTypeDef ADC_VOLT_Handle;

// Private function prototypes ************************************************
static void statusMonitorTask( void *pvParameters );

// Functions ******************************************************************
// ----------------------------------------------------------------------------
/// \brief     Initialise adc peripherals for measuring the microntrollers
///            voltage and temperature. Also Init the monitoring task in the 
///            function, which reads out the adc data register and calculates
///            voltage and temperature in defined time steps.
///
/// \param     none
///
/// \return    none
void monitor_init( void )
{
   ADC_ChannelConfTypeDef     sConfig = {0};
   
   // enable adc clock
   __HAL_RCC_ADC1_CLK_ENABLE();

   //ADC init
   ADC_Handle.Instance                    = ADC1;
   ADC_Handle.Init.ClockPrescaler         = ADC_CLOCK_SYNC_PCLK_DIV4;
   ADC_Handle.Init.Resolution             = ADC_RESOLUTION_12B;
   ADC_Handle.Init.ScanConvMode           = DISABLE;
   ADC_Handle.Init.ContinuousConvMode     = DISABLE;
   ADC_Handle.Init.DiscontinuousConvMode  = DISABLE;
   ADC_Handle.Init.ExternalTrigConvEdge   = ADC_EXTERNALTRIGCONVEDGE_NONE;
   ADC_Handle.Init.ExternalTrigConv       = ADC_SOFTWARE_START;
   ADC_Handle.Init.DataAlign              = ADC_DATAALIGN_RIGHT;
   ADC_Handle.Init.NbrOfConversion        = 1;
   ADC_Handle.Init.DMAContinuousRequests  = DISABLE;
   ADC_Handle.Init.EOCSelection           = ADC_EOC_SINGLE_CONV;
   if (HAL_ADC_Init(&ADC_Handle) != HAL_OK)
   {
      return;
   }

   // start the status monitoring task
   osThreadNew( statusMonitorTask, NULL, &monitorTask_attributes );
}

// ----------------------------------------------------------------------------
/// \brief     DeInitialise adc peripherals.
///
/// \param     none
///
/// \return    none
void monitor_deinit( void )
{
   __HAL_RCC_ADC1_CLK_DISABLE();
   HAL_ADC_DeInit(&ADC_Handle);
}

//-----------------------------------------------------------------------------
/// \brief     The monitoring task reads the adc data register for the
///            calculation of the microcontrollers voltage and temperature.
///            The task is blocked for defined times.
///
/// \param     [in]  pvParameters
///
/// \return    none
static void statusMonitorTask( void *pvParameters )
{
   static volatile uint32_t   ADC_Value;
   static volatile float      mvSensing;
   ADC_ChannelConfTypeDef     sConfig = {0};
   static uint16_t test;
   
   while (1)
   {
      // Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
      sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
      sConfig.Rank = 1;
      sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
      if (HAL_ADC_ConfigChannel(&ADC_Handle, &sConfig) != HAL_OK)
      {
         return;
      }

      // get current temperature
      HAL_ADC_Start(&ADC_Handle);

      if(HAL_ADC_PollForConversion(&ADC_Handle, 100) == HAL_OK)
      {
         ADC_Value = (&ADC_Handle)->Instance->DR;
         HAL_ADC_Stop(&ADC_Handle);
         mvSensing = 1000 * ((ADC_REFERENCE_VOLTAGE_V * ADC_Value) / (ADC_MAX_OUTPUT_VALUE+1.0f));
         temperature = (mvSensing-TEMP_SENSOR_VOLTAGE_MV_AT_25)/TEMP_SENSOR_AVG_SLOPE_MV_PER_CELSIUS + 20.0f;
      }

      // Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
      sConfig.Channel      = ADC_CHANNEL_VREFINT;
      sConfig.Rank         = 1;
      sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
      if (HAL_ADC_ConfigChannel(&ADC_Handle, &sConfig) != HAL_OK)
      {
         return;
      }
      
      // get current temperature
      HAL_ADC_Start(&ADC_Handle);

      if(HAL_ADC_PollForConversion(&ADC_Handle, 100) == HAL_OK)
      {
         ADC_Value = (&ADC_Handle)->Instance->DR;
         HAL_ADC_Stop(&ADC_Handle);
         voltage = 3.3f * ((float)VREF_CAL/(float)ADC_Value);
      }

      // measure temperature all 10 seconds
      vTaskDelay(10000);
   }
}

//-----------------------------------------------------------------------------
/// \brief     Returns temperature from last adc readout and calculation.
///
/// \param     none
///
/// \return    float Temperature
float monitor_getTemperature( void )
{
   return temperature;
}

//-----------------------------------------------------------------------------
/// \brief     Returns internal stm32l431 temperature from last measurement.
///
/// \param     none
///
/// \return    float Voltage
float monitor_getVoltage( void )
{
   return voltage;
}