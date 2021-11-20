// ****************************************************************************
/// \file      usb.c
///
/// \brief     USB device C source file
///
/// \details   Application level usb c source file. Initialises USB stack and
///            the rndis device.
///
/// \author    Nico Korn
///
/// \version   0.3.0.2
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

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_rndis.h"
#include "queuex.h"

// Private defines ************************************************************

// Private types     **********************************************************

// Private variables **********************************************************
static RNDIS_USB_STATISTIC_t rndis_statistic;

// Global variables ***********************************************************
USBD_HandleTypeDef         hUsbDeviceFS = {0};  // USB Device Core handle declaration
extern queue_handle_t      tcpQueue;
extern queue_handle_t      usbQueue;

// Private function prototypes ************************************************

// Functions ******************************************************************
/**
  * Init USB device Library, add supported class and start the library
  * @retval None
  */
void usb_init( void )
{
   // force enum
   usb_forceHostEnum();
   
   // Init Device Library, add supported class and start the library.
   if( USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK )
   {
      Error_Handler();
   }
   if( USBD_RegisterClass(&hUsbDeviceFS, USBD_RNDIS_getClass() ) != USBD_OK )
   {
      Error_Handler();
   }
   if( USBD_Start(&hUsbDeviceFS) != USBD_OK )
   {
      Error_Handler();
   }
}

// ----------------------------------------------------------------------------
/// \brief     Usb init.
///
/// \param     none
///
/// \return    none
void usb_deinit( void )
{   
   USBD_DeInit( &hUsbDeviceFS );
}

// ----------------------------------------------------------------------------
/// \brief     Called if a complete frame has been received.
///
/// \param     [in]  const char *data
/// \param     [in]  int size
///
/// \return    none
inline void on_usbOutRxPacket(const char *data, int size)
{
   rndis_statistic.counterRxFrame++;
   rndis_statistic.counterRxData+=(uint32_t)size;
   queue_enqueue( (uint8_t*)data, size, &usbQueue );
   USBD_RNDIS_setBuffer( queue_getHeadBuffer( &usbQueue ) );
}

// ----------------------------------------------------------------------------
/// \brief     Called if a frame has been send.
///
/// \param     none
///
/// \return    none
inline void on_usbInTxCplt( void )
{
   queue_dequeue(&tcpQueue);
}

// ----------------------------------------------------------------------------
/// \brief     Start a new usb transmission.
///
/// \param     [in]  uint8_t* dpointer
/// \param     [in]  uint16_t length
///
/// \return    none
uint8_t usb_output( uint8_t* dpointer, uint16_t length )
{
   if(!USBD_RNDIS_send(dpointer, length))
   {
      return 0;
   }
   rndis_statistic.counterTxFrame++;
   rndis_statistic.counterTxData+=(uint32_t)length;
   return 1;
}

// ----------------------------------------------------------------------------
/// \brief     Pull D+ down to trigger an enum process by the host
///
/// \param     none
///
/// \return    none
void usb_forceHostEnum( void )
{
  GPIO_InitTypeDef GPIO_Init_Struct; 
  
   // set GPIO preipheral clock
   __HAL_RCC_GPIOA_CLK_ENABLE();

  // init as push pull
  GPIO_Init_Struct.Pin     = GPIO_PIN_12;
  GPIO_Init_Struct.Speed   = GPIO_SPEED_FREQ_HIGH;
  GPIO_Init_Struct.Mode    = GPIO_MODE_OUTPUT_PP;
  GPIO_Init_Struct.Pull    = GPIO_PULLDOWN ;
  HAL_GPIO_Init(GPIOA, &GPIO_Init_Struct);  
   
   // set bit
   GPIOA->BSRR = (uint32_t)GPIO_PIN_12;
 
   // delay to force host to start enummeration
   static uint32_t i=0;
   for(uint32_t t=0; t<100000; t++)
   {
      i++;
   }
   
   // reset bit
   GPIOA->BSRR = (uint32_t)GPIO_PIN_12<<16u;
   
   // init as opendrain
   GPIO_Init_Struct.Speed  = GPIO_SPEED_FREQ_HIGH;
   GPIO_Init_Struct.Mode   = GPIO_MODE_OUTPUT_OD;
   GPIO_Init_Struct.Pull   = GPIO_PULLDOWN;
   HAL_GPIO_Init(GPIOA, &GPIO_Init_Struct);  
}

// ----------------------------------------------------------------------------
/// \brief     Return transmitted frames.
///
/// \param     none
///
/// \return    uint32_t tx frames
uint32_t usb_getTxFrames( void )
{
   return rndis_statistic.counterTxFrame;
}

// ----------------------------------------------------------------------------
/// \brief     Return transmitted data.
///
/// \param     none
///
/// \return    uint32_t tx data
uint32_t usb_getTxData( void )
{
   return rndis_statistic.counterTxData;
}

// ----------------------------------------------------------------------------
/// \brief     Return received frames.
///
/// \param     none
///
/// \return    uint32_t rx frames
uint32_t usb_getRxFrames( void )
{
   return rndis_statistic.counterRxFrame;
}

// ----------------------------------------------------------------------------
/// \brief     Return received data.
///
/// \param     none
///
/// \return    uint32_t rx data
uint32_t usb_getRxData( void )
{
   return rndis_statistic.counterRxData;
}

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/
