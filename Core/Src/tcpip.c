// ****************************************************************************
/// \file      pcu_bus_uart_tcp.c
///
/// \brief     TCP/IP Stack Application Level C Source File
///
/// \details   Module for the handling of the freertos tcp ip stack.
///
/// \author    Nico Korn
///
/// \version   0.3.0.1
///
/// \date      08112021
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
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "tcpip.h"
#include "httpserver.h"
#include "dhcpserver.h"
#include "dnsserver.h"
#include "queuex.h"
#include "main.h"

#include "cmsis_os.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"

// Private define *************************************************************

// Private types     **********************************************************
typedef struct FRAME_s
{
   uint8_t *data;
   uint16_t length;
} FRAME_t;
typedef struct MAC_STATISTIC_s
{
   uint32_t counterTxError;
   uint32_t counterRxFrame;
   uint32_t counterTxFrame;
}MAC_STATISTIC_t;

// Global variables ***********************************************************
const uint8_t ucIPAddressFLASH[4]         = {IP1, IP2, IP3, IP4};
const uint8_t ucMACAddressFLASH[6]        = {MAC_HWADDR};    
const uint8_t ucNetMaskFLASH[4]           = {255, 255, 255, 0};
const uint8_t ucGatewayAddressFLASH[4]    = {IP1, IP2, IP3, IP4};   
const uint8_t ucDNSServerAddressFLASH[4]  = {IP1, IP2, IP3, IP4};    

// Private variables **********************************************************
// Use by the pseudo random number generator.
static MAC_STATISTIC_t  mac_statistic;
static const char       *mainHOST_NAME                   = {HOSTNAME};
static const char       *mainDEVICE_NICK_NAME            = {DEVICENAME};
static const char       *mainHOST_NAMEcapLetters         = {HOSTNAMECAP};
static const char       *mainDEVICE_NICK_NAMEcapLetters  = {DEVICENAMECAP};
static FlagStatus       processingIdle = SET;
static FRAME_t          currentFrame;
extern queue_handle_t   tcpQueue;
extern queue_handle_t   usbQueue;
static leasetableObj_t  leasetable[DHCPPOOLSIZE] =
{
    /* mac    ip address        subnet mask        lease time */
    { {0}, {IP1, IP2, IP3, 2}, {SUB1, SUB2, SUB3, SUB4}, 24 * 60 * 60 },
    { {0}, {IP1, IP2, IP3, 3}, {SUB1, SUB2, SUB3, SUB4}, 24 * 60 * 60 },
    { {0}, {IP1, IP2, IP3, 4}, {SUB1, SUB2, SUB3, SUB4}, 24 * 60 * 60 },
    { {0}, {IP1, IP2, IP3, 5}, {SUB1, SUB2, SUB3, SUB4}, 24 * 60 * 60 }
};
static dhcpconf_t dhcpconf =
{
    {IP1, IP2, IP3, IP4},        // dhcp server address
    {IP1, IP2, IP3, IP4},        // dns server address
    {SUB1, SUB2, SUB3, SUB4},    // subnet address
    "go",                        // domain suffic
    DHCPPOOLSIZE,                // lease table size
    leasetable                   // pointer to lease table
};

osThreadId_t tcpip_macTaskToNotify;
const osThreadAttr_t macReceiveTask_attributes = {
  .name = "MAC-task",
  .stack_size = configMINIMAL_STACK_SIZE * 4,
  .priority = (osPriority_t) osPriorityHigh7,
};

// Private function prototypes ************************************************
static void       tcpip_rngInit             ( void );
static void       tcpip_rngDeInit           ( void );
static uint32_t   tcpip_getRandomNumber     ( void );
static void       tcpip_macTask             ( void *pvParameters );
static void       tcpip_invokeMacTask       ( void );

// Functions ******************************************************************
// ----------------------------------------------------------------------------
/// \brief     Initialise the TCP/IP stack.
///
/// \param     none
///
/// \return    none
void tcpip_init( void )
{   
   // init random number generator
   tcpip_rngInit();
   
   // initialise the TCP/IP stack.
   FreeRTOS_IPInit( ucIPAddressFLASH, ucNetMaskFLASH, ucGatewayAddressFLASH, ucDNSServerAddressFLASH, ucMACAddressFLASH );  
   
   // initialise receive complete task
   tcpip_macTaskToNotify = osThreadNew( tcpip_macTask, NULL, &macReceiveTask_attributes );
}

// ----------------------------------------------------------------------------
/// \brief     Delete all running ip tasks.
///
/// \param     none
///
/// \return    none
void tcpip_deinit( void )
{
   osThreadTerminate(&tcpip_macTaskToNotify);
   httpserver_deinit();
   dhcpserver_deinit();
   dnsserver_deinit();
   tcpip_rngDeInit();
}

//-----------------------------------------------------------------------------
/// \brief     Called by FreeRTOS+TCP when the network connects or disconnects.  
///            Disconnect events are only received if implemented in the MAC 
///            driver.
///
/// \param     [in]  eIPCallbackEvent_t eNetworkEvent
///
/// \return    none
void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
   static BaseType_t xTasksAlreadyCreated = pdFALSE;

	// If the network has just come up...
	if( eNetworkEvent == eNetworkUp )
	{
		// ...create the tasks that use the IP stack if they have not already been
		// created.
		if( xTasksAlreadyCreated == pdFALSE )
		{
         // start webserver
         httpserver_init();
         
         // start dhcp server
         dhcpserver_init(&dhcpconf);
         
         // start dns server
         dnsserver_init();

         // set the task created flag
         xTasksAlreadyCreated = pdTRUE;
		}
	}
}

//-----------------------------------------------------------------------------
/// \brief     Utility wrapper function to generate a random number.
///
/// \param     none
///
/// \return    UBaseType_t random number
UBaseType_t uxRand( void )
{
	return tcpip_getRandomNumber();
}

//-----------------------------------------------------------------------------
/// \brief     Function used by the FreeRTOS tcp ip stack.
///
/// \param     none
///
/// \return    UBaseType_t random number
extern BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber )
{
   *pulNumber = tcpip_getRandomNumber();
   return pdTRUE;
}

//-----------------------------------------------------------------------------
/// \brief     Seed random numbers to functions. Function given by TCP/IP stack
///            and uses a function which returns a hardware generated random
///            number.
///
/// \param     [in]  uint32_t ulSourceAddress
///            [in]  uint16_t usSourcePort
///            [in]  uint32_t ulDestinationAddress
///            [in]  uint16_t usDestinationPort
///
/// \return    uint32_t random number from function uxRand()
extern uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress, uint16_t usSourcePort, uint32_t ulDestinationAddress, uint16_t usDestinationPort )
{
	( void ) ulSourceAddress;
	( void ) usSourcePort;
	( void ) ulDestinationAddress;
	( void ) usDestinationPort;

	return uxRand();
}

//-----------------------------------------------------------------------------
/// \brief     Returns char pointer to device hostname. Function header is
///            provided by the TCP/IP stack.
///
/// \param     none
///
/// \return    const char* pointer to device hostname
#if( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 ) || ( ipconfigDHCP_REGISTER_HOSTNAME == 1 )
const char *pcApplicationHostnameHook( void )
{
   return mainDEVICE_NICK_NAME;
}

//-----------------------------------------------------------------------------
/// \brief     Returns char pointer to device hostname. Function header is
///            provided by the TCP/IP stack.
///
/// \param     none
///
/// \return    const char* pointer to device hostname with capital letters
const char *pcApplicationHostnameHookCAP( void )
{
   return mainDEVICE_NICK_NAMEcapLetters;
}
#endif

//-----------------------------------------------------------------------------
/// \brief     Function to resolve device name on request by a client. Function
///            header provided by the TCP/IP stack.
///
/// \param     [in]  const char *pcName
///
/// \return    BaseType_t xReturn
#if( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )
BaseType_t xApplicationDNSQueryHook( const char *pcName )
{
   static uint32_t dnscalledCounter;
   static uint32_t dnssuccessCounter;
   static uint32_t dnsfailCounter;
	BaseType_t     xReturn;
   const char* temp = pcName;
   dnscalledCounter++;

   // Determine if a name lookup is for this node.  Two names are given
   // to this node: that returned by pcApplicationHostnameHook() and that set
   // by mainDEVICE_NICK_NAME. If its a ip address query, check the ip address.
   if(      strcmp( temp, pcApplicationHostnameHook() ) == 0 
         || strcmp( temp, mainDEVICE_NICK_NAMEcapLetters ) == 0 
         || strcmp( temp, mainDEVICE_NICK_NAME ) == 0 
         || strcmp( temp, HOSTNAMEDNS ) == 0 )
   {
      dnssuccessCounter++;
      xReturn = pdPASS;
   }
   else
   {
      static char    ipAddrQuery[30];
      uint32_t       ipAddress;
      uint8_t        ip1DigitCnt = 1;
      uint8_t        ip1;
      uint8_t        ip2DigitCnt = 1;
      uint8_t        ip2;
      uint8_t        ip3DigitCnt = 1;
      uint8_t        ip3;
      uint8_t*       ipAddress8b;
      
      // get ip configuration
      FreeRTOS_GetAddressConfiguration( &ipAddress, NULL, NULL, NULL );
      ipAddress8b = (uint8_t*)(&ipAddress);
      
      // prepare string to be compared with query
      // get digit counts
      ip1 = ipAddress8b[0];
      while( ip1 >= 10 && ip1DigitCnt<4 )
      {
         ip1 /= 10;
         ip1DigitCnt++;
      }

      ip2 = ipAddress8b[1];
      while( ip2 >= 10 && ip2DigitCnt<4 )
      {
         ip2 /= 10;
         ip2DigitCnt++;
      }

      ip3 = ipAddress8b[2];
      while( ip3 >= 10 && ip3DigitCnt<4 )
      {
         ip3 /= 10;
         ip3DigitCnt++;
      }
         
      memset(ipAddrQuery, 0x00, 30);
      sprintf( ipAddrQuery, "%d%c%d%c%d%c%d\ain-addr%carpa", ipAddress8b[3], ip3DigitCnt, ipAddress8b[2], ip2DigitCnt, ipAddress8b[1], ip1DigitCnt, ipAddress8b[0], 0x04 );
   
      if( strcmp( temp, ipAddrQuery ) == 0 )
      {
         dnssuccessCounter++;
         xReturn = pdPASS;
      }
      else
      {
         dnsfailCounter++;
         xReturn = pdFAIL;
      }
   }

   return xReturn;
}
#endif

//------------------------------------------------------------------------------
/// \brief     Tcp start output/transmit function.          
///
/// \param     [in] uint8_t* buffer
/// \param     [in] uint16_t length
///
/// \return    0 = not send, 1 = send
uint8_t tcpip_output( uint8_t* buffer, uint16_t length )
{
   if( buffer == NULL || length == 0 )
   {
      queue_dequeue( &usbQueue );
      return 0;
   }

   if( processingIdle != RESET )
   {
      // set states to processing new frame from the mac layer
      currentFrame.data = buffer;
      currentFrame.length = length;
      processingIdle = RESET;
      tcpip_invokeMacTask();
      return 1;
   }
   
   return 0;
}

// ----------------------------------------------------------------------------
/// \brief     Mac main task. The task gets a frame from the fifo and hands it
///            over to the TCP/IP stack.
///
/// \param     [in]  void *pvParameters
///
/// \return    none
static void tcpip_macTask( void *pvParameters )
{
   // current step get pointer and length of frame!
   NetworkBufferDescriptor_t  *pxBufferDescriptor;
   size_t                     xBytesReceived;
   uint8_t*                   xFramePointer;
   // Used to indicate that xSendEventStructToIPTask() is being called because of an Ethernet receive event.
   IPStackEvent_t             xRxEvent;
   static uint32_t            rxMacCallCounter, rxMacErrCounter;

   for( ;; )
   {
      /* Wait for the Ethernet MAC interrupt to indicate that another packet
      has been received.  The task notification is used in a similar way to a
      counting semaphore to count Rx events, but is a lot more efficient than
      a semaphore. */
      ulTaskNotifyTake( pdFALSE, portMAX_DELAY );
        
      rxMacCallCounter++;

      // set the bytes received variable
      xBytesReceived = currentFrame.length;
      xFramePointer = currentFrame.data;
      
      /* Allocate a network buffer descriptor that points to a buffer
      large enough to hold the received frame.  As this is the simple
      rather than efficient example the received data will just be copied
      into this buffer. */
      pxBufferDescriptor = (NetworkBufferDescriptor_t*)pxGetNetworkBufferWithDescriptor( xBytesReceived, 0 );
      
      if( pxBufferDescriptor != NULL )
      {
         // copy data onto the heap release frame from fifo
         memcpy(pxBufferDescriptor->pucEthernetBuffer, (uint8_t*)xFramePointer, xBytesReceived);
         
         // set the pointer back
         pxBufferDescriptor->xDataLength = xBytesReceived;
         
         /* See if the data contained in the received Ethernet frame needs
         to be processed.  NOTE! It is preferable to do this in
         the interrupt service routine itself, which would remove the need
         to unblock this task for packets that don't need processing. */
         if( eConsiderFrameForProcessing( pxBufferDescriptor->pucEthernetBuffer ) == eProcessBuffer )
         {
            /* The event about to be sent to the TCP/IP is an Rx event. */
            xRxEvent.eEventType = eNetworkRxEvent;
            
            /* pvData is used to point to the network buffer descriptor that
            now references the received data. */
            xRxEvent.pvData = ( void * ) pxBufferDescriptor;
            
            /* Send the data to the TCP/IP stack. */
            if( xSendEventStructToIPTask( &xRxEvent, 0 ) == pdFALSE )
            {
               /* The buffer could not be sent to the IP task so the buffer
               must be released. */
               vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );
               
               /* Make a call to the standard trace macro to log the
               occurrence. */
               iptraceETHERNET_RX_EVENT_LOST();
            }
            else
            {
               /* The message was successfully sent to the TCP/IP stack.
               Call the standard trace macro to log the occurrence. */
               iptraceNETWORK_INTERFACE_RECEIVE();
            }
         }
         else
         {
            /* The Ethernet frame can be dropped, but the Ethernet buffer
            must be released. */
            vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );
         }
      }
      else
      {
         /* The event was lost because a network buffer was not available.
         Call the standard trace macro to log the occurrence. */
         iptraceETHERNET_RX_EVENT_LOST();
         rxMacErrCounter++;
      }

      // reset the processing frame flag
      processingIdle = SET;
      mac_statistic.counterTxFrame++;  // this is likely to send a frame from rndis view
      queue_dequeue( &usbQueue );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Function used to unblock the MAC task.
///
/// \param     none
///
/// \return    none
static void tcpip_invokeMacTask( void )
{
   if( tcpip_macTaskToNotify != NULL )
   {
      // Notify to start the emac task to process the next frame from fifo
      xTaskNotifyGive( tcpip_macTaskToNotify );
   }
}

//------------------------------------------------------------------------------
/// \brief     Called if a call to pvPortMalloc() fails because there is 
///            insufficient free memory available in the FreeRTOS heap.  
///            pvPortMalloc() is called internally by FreeRTOS API functions 
///            that create tasks, queues, software timers, and semaphores.  
///            The size of the FreeRTOS heap is set by the configTOTAL_HEAP_SIZE 
///            configuration constant in FreeRTOSConfig.h. 
///
/// \param     none
///
/// \return    none
void vApplicationMallocFailedHook( void )
{
   static uint32_t malloc_fail_counter = 0;
   malloc_fail_counter++;
}

//------------------------------------------------------------------------------
/// \brief     If ipconfigSUPSBLINK_OUTGOING_PINGS is set to 1 in 
///            FreeRTOSIPConfig.h then vApplicationPingReplyHook() is 
///            called by the IP stack when the stack receives a
///            ping reply.
///
/// \param     [in]  ePingReplyStatus_t eStatus
/// \param     [in]  uint16_t usIdentifier
///
/// \return    none
void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
    switch( eStatus )
    {
        case eSuccess    :
            // A valid ping reply has been received.  Post the sequence number
            // on the queue that is read by the vSendPing() function below.  Do
            // not wait more than 10ms trying to send the message if it cannot be
            // sent immediately because this function is called from the IP stack
            // task - blocking in this function will block the IP stack.
            break;

        case eInvalidChecksum :
        case eInvalidData :
            // A reply was received but it was not valid.
            break;
    }
}

//------------------------------------------------------------------------------
/// \brief     Since the stm32f411 doesn't has a RNG peripheral we have to
///            work with pseudo random numbers. The seed is are the wafer coord-
///            inates of this microcontroller stored at address 0x48CD.
///
/// \param     none
///
/// \return    none
static void tcpip_rngInit( void )
{
   srand(  (uint32_t)((const uint32_t *)0x48CD)  );
}

//------------------------------------------------------------------------------
/// \brief     Deinit function of the random number generator peripherals.
///
/// \param     none
///
/// \return    none
static void tcpip_rngDeInit( void )
{
   
}

//------------------------------------------------------------------------------
/// \brief     Function which returns a hardware generated random number.
///
/// \param     none
///
/// \return    uint32_t random number
static uint32_t tcpip_getRandomNumber( void )
{
   return rand()%0xffffffff;
}

//------------------------------------------------------------------------------
/// \brief     Function to handover a message from the tcp ip stack to the 
///            queue.
///
/// \param     none
///
/// \return    0 = queue is full, 1 = frame queued
uint8_t tcpip_enqueue( uint8_t* data, uint16_t length )
{
   if( queue_isFull( &tcpQueue ) != 1 )
   {
      return 0;
   }
   
   // local variables
   //uint32_t   crc32;
   //uint8_t*   crcFragment;

   // copy message into queue header, as this is being interpreted as received message on secondary output
   uint8_t *rxBuffer = queue_getHeadBuffer( &tcpQueue ) + RXBUFFEROFFSET;
   
   // copy data into buffer
   memcpy( rxBuffer, data, length );
   
   //// calculate crc32
   //crc32 = crc32_calculate( (uint32_t*)&rxBuffer[0], (uint32_t)length );
   //
   //// append crc to the outputbuffer
   //crcFragment = (uint8_t*)&crc32;
   //
   //for( uint8_t i=0, j=3; i<4; i++,j-- )
   //{
   //   *(rxBuffer+length+i) = *(crcFragment+j);
   //}
   
   // this is likely to receive a frame on the rndis part
   mac_statistic.counterRxFrame++;
   
   // enqueue to the ringbuffer
   queue_enqueue( rxBuffer, (uint16_t)(length), &tcpQueue );
   
   return 1;
}