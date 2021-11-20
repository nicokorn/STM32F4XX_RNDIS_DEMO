// ****************************************************************************
/// \file      usb.h
///
/// \brief     USB device C header file
///
/// \details   Application level usb c source file. Initialises USB stack and
///            the rndis device.
///
/// \author    Nico Korn
///
/// \version   0.1.1.0
///
/// \date      21102021
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

// Define to prevent recursive inclusion **************************************
#ifndef __USB_DEVICE__H__
#define __USB_DEVICE__H__

// Include ********************************************************************
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "usbd_def.h"

// Exported defines ***********************************************************
    
// Exported types *************************************************************
    
// Exported functions *********************************************************
void     usb_init                ( void );
void     usb_deinit              ( void );
void     on_usbOutRxPacket       ( const char *data, int size );
void     on_usbInTxCplt          ( void );
uint8_t  usb_output              ( uint8_t* dpointer, uint16_t length );
void     usb_forceHostEnum       ( void );
uint32_t usb_getTxFrames         ( void );
uint32_t usb_getTxData           ( void );
uint32_t usb_getRxFrames         ( void );
uint32_t usb_getRxData           ( void );

#endif /* __USB_DEVICE__H__ */

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/
