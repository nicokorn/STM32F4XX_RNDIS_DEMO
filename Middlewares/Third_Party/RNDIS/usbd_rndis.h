// ****************************************************************************
/// \file      usbd_rndis.h
///
/// \brief     RNDIS device C header file
///
/// \details   The rndis usb device c source file contains the top level layer
///            of the usb device functionality.
///
/// \author    Nico Korn
///
/// \version   0.3.0.2 - experimental, not yet released
///
/// \date      29102021
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
#ifndef __USBD_RNDIS_H_
#define __USBD_RNDIS_H_

// Include ********************************************************************
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "usbd_ioreq.h"

// Exported defines ***********************************************************
#define RNDIS_MTU        1500                           /* MTU value */
#define RNDIS_LINK_SPEED 12000000                       /* Link baudrate (12Mbit/s for USB-FS) */
#define RNDIS_VENDOR     "fetisov"                      /* NIC vendor name */
//#define RNDIS_HWADDR     0x20,0x89,0x84,0x6A,0x96,0xAB  /* MAC-address to set to host interface */
#define RNDIS_HWADDR     0xDA, 0xBE, 0xEF, 0x15, 0xDE, 0xAD  /* MAC-address to set to host interface */
#define CDC_DATA_HS_MAX_PACKET_SIZE                 512U  /* Endpoint IN & OUT Packet size */
#define CDC_DATA_FS_MAX_PACKET_SIZE                 64U  /* Endpoint IN & OUT Packet size */
    
#define CDC_DATA_HS_IN_PACKET_SIZE                  CDC_DATA_HS_MAX_PACKET_SIZE
#define CDC_DATA_HS_OUT_PACKET_SIZE                 CDC_DATA_HS_MAX_PACKET_SIZE
    
#define CDC_DATA_FS_IN_PACKET_SIZE                  CDC_DATA_FS_MAX_PACKET_SIZE
#define CDC_DATA_FS_OUT_PACKET_SIZE                 CDC_DATA_FS_MAX_PACKET_SIZE

// Exported types *************************************************************

typedef struct _USBD_RNDIS_Itf
{
  int8_t (* Init)(void);
  int8_t (* DeInit)(void);
  int8_t (* Control)(uint8_t cmd, uint8_t *pbuf, uint16_t length);
  int8_t (* Receive)(uint8_t *Buf, uint32_t *Len);
  int8_t (* TransmitCplt)(uint8_t *Buf, uint32_t *Len, uint8_t epnum);
} USBD_RNDIS_ItfTypeDef;

//-----------------------------------------------------------------------------
/// \brief     Bus uart datastructure for frame statistics.										   
typedef struct BUS_UART_STATISTIC_s
{
   uint32_t                      counterRxFrame;               ///< counter for valid frames
   uint32_t                      counterTxFrame;               ///< counter for valid frames
}RNDIS_USB_STATISTIC_t;

// Exported functions *********************************************************
bool                 USBD_RNDIS_canSend            ( void );
bool                 USBD_RNDIS_send               ( const void *data, uint16_t size );
void                 USBD_RNDIS_setBuffer          ( uint8_t* buffer );
USBD_ClassTypeDef*   USBD_RNDIS_getClass           ( void );
uint8_t              USBD_RNDIS_RegisterInterface  ( USBD_HandleTypeDef *pdev, USBD_RNDIS_ItfTypeDef *fops );
#endif

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/