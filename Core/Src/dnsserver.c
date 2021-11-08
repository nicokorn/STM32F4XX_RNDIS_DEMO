// ****************************************************************************
/// \file      dnsserver.c
///
/// \brief     dns server c file
///
/// \details   Handles dns requests from clients.
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
#include  <string.h>
#include  <stdlib.h>
#include "dnsserver.h"
#include "tcpip.h"
#include "printf.h"

#include "cmsis_os.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_DHCP.h"

// Private defines ************************************************************
#define TXRXBUFFERSIZE              ( 650u )
#define MAX_LENGTH                  ( 50u )
#define FLAG_RECURSIONDESIRED       ( 0x0001 )
#define FLAG_TRUNCATION             ( 0x0002 )
#define FLAG_AUTHANSWER             ( 0x0004 )
#define FLAG_OPCODE                 ( 0x0078 )
#define FLAG_QRESPONSE              ( 0x0080 )
#define FLAG_RESPCODE               ( 0x0F00 )
#define FLAG_ZERO                   ( 0x7000 )
#define FLAG_RECUSRIONAVAIL         ( 0x8000 )

// Private types     **********************************************************
typedef __packed struct DNS_HEADER_s
{
	uint16_t    id;
	uint16_t    flags;
	uint16_t    n_record[4];
} DNS_HEADER_t;

typedef __packed struct DNS_RESPONSE_s
{
	uint16_t    name;
	uint16_t    type;
	uint16_t    dnsclass;
	uint32_t    ttl;
	uint16_t    len;
	uint32_t    addr;
} DNS_RESPONSE_t;

typedef __packed struct DNS_QUERY_s
{
	char name[MAX_LENGTH];
   uint16_t length;
   uint16_t labelnr;
	uint16_t type;
	uint16_t dnsclass;
} DNS_QUERY_t;

// Private variables **********************************************************
osThreadId_t dnsserverHandleTaskToNotify;
const osThreadAttr_t dnsserverHandleTask_attributes = {
  .name = "DNS-task",
  .stack_size = 4 * configMINIMAL_STACK_SIZE * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
static void dnsserver_handle( void *pvParameters );

// Global variables ***********************************************************

// Private function prototypes ************************************************
static void       dnsserver_handle     ( void *pvParameters );
static uint8_t    dnsserver_parseQuery ( uint8_t *data, uint16_t length, DNS_QUERY_t *query );

// Functions ******************************************************************

//------------------------------------------------------------------------------
/// \brief     Dns server initialization function.
///
/// \param     none
///
/// \return    none
void dnsserver_init( void )
{
   // initialise dns handle task
   dnsserverHandleTaskToNotify = osThreadNew( dnsserver_handle, NULL, &dnsserverHandleTask_attributes );
}

//------------------------------------------------------------------------------
/// \brief     Dns server deinitialization function.
///
/// \param     none
///
/// \return    none
void dnsserver_deinit( void )
{
   osThreadTerminate(&dnsserverHandleTaskToNotify);
}

// ----------------------------------------------------------------------------
/// \brief     The dns handle task listens on udp port 53 for incoming dns
///            requests.
///
/// \param     [in]  void *pvParameters
///
/// \return    none
static void dnsserver_handle( void *pvParameters )
{
   uint8_t           *pucTxRxBuffer;
   BaseType_t        lengthOfbytes;
   static uint32_t   errorDisco;
   static uint32_t   errorReq;
   uint8_t*          ptr;
   static uint16_t   etimeout;  
   static uint16_t   enomem;  
   static uint16_t   enotconn;
   static uint16_t   eintr;   
   static uint16_t   einval; 
   static uint16_t   eelse;
   long              lBytes;
   struct            freertos_sockaddr xClient, xBindAddress;
   uint32_t          xClientLength = sizeof( xClient );
   Socket_t          xListeningSocket;
   struct            freertos_sockaddr xDestinationAddress;
   DNS_QUERY_t       dnsQuery;
   
   // allocate heap for the transmit and receive message
	pucTxRxBuffer = ( uint8_t * ) pvPortMalloc( TXRXBUFFERSIZE );
   
   if( pucTxRxBuffer == NULL )
   {
      vTaskDelete( NULL );
   }
   
   /* Attempt to open the socket. */
   xListeningSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP );

   /* Check the socket was created. */
   configASSERT( xListeningSocket != FREERTOS_INVALID_SOCKET );

   /* Bind to port 53 for dns messages */
   xBindAddress.sin_port = FreeRTOS_htons( 53 );
   FreeRTOS_bind( xListeningSocket, &xBindAddress, sizeof( xBindAddress ) );

   for( ;; )
   {
       /* Receive data from the socket.  ulFlags is zero, so the standard
       interface is used.  By default the block time is portMAX_DELAY, but it
       can be changed using FreeRTOS_setsockopt(). */
       lengthOfbytes = FreeRTOS_recvfrom( xListeningSocket,
                                   pucTxRxBuffer,
                                   TXRXBUFFERSIZE,
                                   0,
                                   &xClient,
                                   &xClientLength );

      // check lengthOfbytes ------------- lengthOfbytes > 0                           --> data received
      //                                   lengthOfbytes = 0                           --> timeout
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_ENOMEM     --> not enough memory on socket
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_ENOTCONN   --> socket was or got closed
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_EINTR      --> if the socket received a signal, causing the read operation to be aborted
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_EINVAL     --> socket is not valid
      if( lengthOfbytes > 0 )
      {         
         if( lengthOfbytes <= sizeof(DNS_HEADER_t) )
         {
            continue;
         }

         // extract the name in the payload
         DNS_RESPONSE_t    *dnsresponse;
         DNS_HEADER_t      *dnsheader;
         uint8_t           namequerylength = 0;
         uint8_t           namequrtyparsed[50] = {0};
         uint8_t           *namequery = pucTxRxBuffer + sizeof(DNS_HEADER_t) + 1;
         
         namequerylength = strlen((char const*)namequery);
         if( namequerylength > 50 )
         {
            // name too long
            continue;
         }
         
         // parse the name query
         for( uint8_t i=0; i<49; i++ )
         {
            if( namequery[i] <= 0x3f && namequery[i] > 0u )
            {
               namequrtyparsed[i] = '.';
            }
            else if( namequery[i] == 0 )
            {
               namequrtyparsed[i] = 0;
               break;
            }
            else
            {
               namequrtyparsed[i] = namequery[i];
            }
         }
         
         // compare with host name (this device)
         if( memcmp( namequrtyparsed, HOSTNAMEDNS, strlen(HOSTNAMEDNS) ) != 0 )
         {
            // not our hostname
            continue;
         }
         
         // prepare response to query
         dnsheader               = (DNS_HEADER_t*)pucTxRxBuffer;
         dnsheader->flags        |= (uint16_t)FLAG_QRESPONSE;
         dnsheader->n_record[1]  = FreeRTOS_htons(1);
         dnsresponse             = (DNS_RESPONSE_t*)(pucTxRxBuffer+lengthOfbytes);
         dnsresponse->name       = FreeRTOS_htons(0xC00C);
         dnsresponse->type       = FreeRTOS_htons(1);
         dnsresponse->dnsclass   = FreeRTOS_htons(1);
         dnsresponse->ttl        = FreeRTOS_htonl(32);
         dnsresponse->len        = FreeRTOS_htons(4);
         dnsresponse->addr       = FreeRTOS_inet_addr_quick( IP1, IP2, IP3, IP4 );

         // send the query response
         FreeRTOS_sendto( xListeningSocket, pucTxRxBuffer, lengthOfbytes+sizeof(DNS_RESPONSE_t), 0, &xClient, sizeof( xClient ) );
      }
      else if( lengthOfbytes == 0 )
      {
         // No data was received, but FreeRTOS_recv() did not return an error. Timeout?
         etimeout++;
      }
      else if( lengthOfbytes == pdFREERTOS_ERRNO_ENOMEM )                                                                                        
      {                                                                                                                    
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                   
         enomem++;
      } 
      else if( lengthOfbytes == pdFREERTOS_ERRNO_ENOTCONN )                                                                   
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         enotconn++;
      } 
      else if( lengthOfbytes == pdFREERTOS_ERRNO_EINTR )                                                                      
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         eintr++;
      } 
      else if( lengthOfbytes == pdFREERTOS_ERRNO_EINVAL )                                                                      
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         einval++;
      } 
      else
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         eelse++;
      } 
   }
}
/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/