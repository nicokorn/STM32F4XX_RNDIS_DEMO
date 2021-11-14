// ****************************************************************************
/// \file      tcpip.h
///
/// \brief     TCP/IP stack application level header file
///
/// \details   Module for the handling of the freertos tcp ip stack
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
#ifndef __TCP_H
#define __TCP_H

// Include ********************************************************************
#include "stm32f4xx_hal.h"

// Exported defines ***********************************************************
#define IP1             ( 192u )
#define IP2             ( 168u )
#define IP3             ( 2u )
#define IP4             ( 1u )
#define SUB1            ( 255u )
#define SUB2            ( 255u )
#define SUB3            ( 255u )
#define SUB4            ( 0u )
#define DHCPPOOLSIZE    ( 4u )

#define RXBUFFEROFFSET (uint16_t)(44u) // +44 because of the rndis usb header siz
#define HOSTNAME        "rndis"
#define HOSTNAMECAP     "RNDIS"
#define DEVICENAME      "rndis"
#define DEVICENAMECAP   "RNDIS"
#define HOSTNAMEDNS     "rndis.go"
#define MAC_HWADDR      0xAD, 0xDE, 0x15, 0xEF, 0xBE, 0xDA 

// Exported types *************************************************************

// Exported functions *********************************************************
void                    tcpip_init                    ( void );
void                    tcpip_deinit                  ( void );
uint8_t                 tcpip_output                  ( uint8_t* buffer, uint16_t length );
const char*             pcApplicationHostnameHookCAP  ( void );
uint8_t                 tcpip_enqueue                 ( uint8_t* data, uint16_t length );
#endif // __TCP_H