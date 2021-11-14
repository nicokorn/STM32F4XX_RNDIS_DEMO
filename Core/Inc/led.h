// ****************************************************************************
/// \file      led.h
///
/// \brief     LED C Header File
///
/// \details   Module to control the led which is connected
///            directly to a gpio.
///
/// \author    Nico Korn
///
/// \version   0.3.0.2 - experimental, not yet released
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

// Define to prevent recursive inclusion **************************************
#ifndef __LED_H
#define __LED_H

// Include ********************************************************************
#include "stm32f4xx_hal.h"

// Exported defines ***********************************************************

// Exported types *************************************************************

// Exported functions *********************************************************
void     led_init       ( void );
void     led_deinit     ( void );
void     led_toggle     ( void );
void     led_set        ( void );
void     led_reset      ( void );
void     led_setDuty    ( uint8_t dutyParam );
uint8_t  led_getDuty    ( void );
void     led_test       ( void );
void     led_setPulse   ( void );
void     led_setDim     ( void );
#endif // __LED_H