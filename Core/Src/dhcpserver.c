// ****************************************************************************
/// \file      dhcpserver.c
///
/// \brief     dhcp server c source file
///
/// \details   Handles dhcp requests from clients.
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

// Include ********************************************************************
#include  <string.h>
#include  <stdlib.h>
#include "dhcpserver.h"
#include "printf.h"

#include "cmsis_os.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_DHCP.h"

// Private defines ************************************************************
#define TXRXBUFFERSIZE     ( 650u )

#define DHCP_DISCOVER       ( 1u )
#define DHCP_OFFER          ( 2u )
#define DHCP_REQUEST        ( 3u )
#define DHCP_DECLINE        ( 4u )
#define DHCP_ACK            ( 5u )
#define DHCP_NAK            ( 6u )
#define DHCP_RELEASE        ( 7u )
#define DHCP_INFORM         ( 8u )

// Private types     **********************************************************
typedef struct  DHCP_MSG_t // __packed
{
    uint8_t  op;           // packet opcode type 
    uint8_t  htype;        // hardware addr type 
    uint8_t  hlen;         // hardware addr length 
    uint8_t  hops;         // gateway hops 
    uint32_t xid;          // transaction ID 
    uint16_t secs;         // seconds since boot began 
    uint16_t flags;        // flags
    uint8_t  ciaddr[4];    // client IP address 
    uint8_t  yiaddr[4];    // 'your' IP address 
    uint8_t  siaddr[4];    // server IP address 
    uint8_t  giaddr[4];    // gateway IP address 
    uint8_t  chaddr[16];   // client hardware address 
    uint8_t  legacy[192];  // old fields
    uint8_t  magic[4];     // magic number
    uint8_t  options[275]; // options area
} DHCP_MSG_t;

// Private variables **********************************************************
osThreadId_t dhcpserverHandleTaskToNotify;
const osThreadAttr_t dhcpserverHandleTask_attributes = {
  .name = "DHCP-task",
  .stack_size = 4 * configMINIMAL_STACK_SIZE * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

enum dhcp_options
{
	DHCP_PAD                    = 0,
	DHCP_SUBNETMASK             = 1,
	DHCP_ROUTER                 = 3,
	DHCP_DNSSERVER              = 6,
	DHCP_HOSTNAME               = 12,
	DHCP_DNSDOMAIN              = 15,
	DHCP_MTU                    = 26,
	DHCP_BROADCAST              = 28,
	DHCP_PERFORMROUTERDISC      = 31,
	DHCP_STATICROUTE            = 33,
	DHCP_NISDOMAIN              = 40,
	DHCP_NISSERVER              = 41,
	DHCP_NTPSERVER              = 42,
	DHCP_VENDOR                 = 43,
	DHCP_IPADDRESS              = 50,
	DHCP_LEASETIME              = 51,
	DHCP_OPTIONSOVERLOADED      = 52,
	DHCP_MESSAGETYPE            = 53,
	DHCP_SERVERID               = 54,
	DHCP_PARAMETERREQUESTLIST   = 55,
	DHCP_MESSAGE                = 56,
	DHCP_MAXMESSAGESIZE         = 57,
	DHCP_RENEWALTIME            = 58,
	DHCP_REBINDTIME             = 59,
	DHCP_CLASSID                = 60,
	DHCP_CLIENTID               = 61,
	DHCP_USERCLASS              = 77,  /* RFC 3004 */
	DHCP_FQDN                   = 81,
	DHCP_DNSSEARCH              = 119, /* RFC 3397 */
	DHCP_CSR                    = 121, /* RFC 3442 */
	DHCP_MSCSR                  = 249, /* MS code for RFC 3442 */
	DHCP_END                    = 255
};

static dhcpconf_t             *dhcpconf;
static DHCP_MSG_t   *dhcpMsg;
static TickType_t             xReceiveTimeOut   = pdMS_TO_TICKS( 4000 );
static TickType_t             xSendTimeOut      = pdMS_TO_TICKS( 4000 );
static char                   magic_cookie[]    = {0x63,0x82,0x53,0x63};

// Global variables ***********************************************************

// Private function prototypes ************************************************
static void             dhcpserver_handle          ( void *pvParameters );
static leasetableObj_t  *dhcpserver_lookupIp       ( uint32_t ip );
static leasetableObj_t  *dhcpserver_lookupMac      ( uint8_t *mac );
static leasetableObj_t  *dhcpserver_lookupRegister ( uint8_t *mac, uint8_t *ip );
static void             dhcpserver_lookupSetMac    ( leasetableObj_t* tableObj, uint8_t* mac );
static leasetableObj_t  *dhcpserver_lookupFree     ( void );
static uint8_t          dhcpserver_lookupFreeObj   ( leasetableObj_t* tableObj );
static void             dhcpserver_lookupDelete    ( leasetableObj_t* tableObj );
static void             dhcpserver_fillOptions     ( void *dest, uint8_t msg_type, const char *domain, uint32_t dns, uint32_t lease_time, uint32_t serverid, uint32_t router, uint32_t subnet );
static uint8_t          *dhcpserver_findOption     ( uint8_t *attrs, uint16_t size, uint8_t attr );

// Functions ******************************************************************

//------------------------------------------------------------------------------
/// \brief     Dhcp server initialization function.
///
/// \param     none
///
/// \return    none
void dhcpserver_init( dhcpconf_t *dhcpconf_param )
{
   // register leasing pool
   dhcpconf = dhcpconf_param;
   
   // initialise dhcp handle task
   dhcpserverHandleTaskToNotify = osThreadNew( dhcpserver_handle, NULL, &dhcpserverHandleTask_attributes );
}

//------------------------------------------------------------------------------
/// \brief     Dhcp server deinitialization function.
///
/// \param     none
///
/// \return    none
void dhcpserver_deinit( void )
{
   osThreadTerminate(&dhcpserverHandleTaskToNotify);
}

// ----------------------------------------------------------------------------
/// \brief     The dhcp handle task listens on udp port 67 for incoming dhcp
///            requests.
///
/// \param     [in]  void *pvParameters
///
/// \return    none
static void dhcpserver_handle( void *pvParameters )
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
   struct freertos_sockaddr xDestinationAddress;
   
   // allocate heap for the transmit and receive message
	pucTxRxBuffer = ( uint8_t * ) pvPortMalloc( TXRXBUFFERSIZE );
   
   if( pucTxRxBuffer == NULL )
   {
      vTaskDelete( NULL );
   }
   
   // Attempt to open the socket.
   xListeningSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP );

   // Check the socket was created.
   configASSERT( xListeningSocket != FREERTOS_INVALID_SOCKET );

   // Bind to port 67.
   xBindAddress.sin_port = FreeRTOS_htons( 67 );
   FreeRTOS_bind( xListeningSocket, &xBindAddress, sizeof( xBindAddress ) );

   for( ;; )
   {
       // Receive data from the socket.  ulFlags is zero, so the standard
       // interface is used.  By default the block time is portMAX_DELAY, but it
       // can be changed using FreeRTOS_setsockopt().
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
         // local variables
         leasetableObj_t *leaseObj;
         
         // process dhcp frames
         dhcpMsg = (DHCP_MSG_t*)pucTxRxBuffer;
         
         switch ( dhcpMsg->options[2] )
         {
            case DHCP_DISCOVER:
               // check if mac address has already a leased ip address
               leaseObj = dhcpserver_lookupMac( dhcpMsg->chaddr );
               
               // if no ip address has been found search for a free ip address
               if( leaseObj == NULL )
               {
                  leaseObj = dhcpserver_lookupFree();
               }
               
               // if the are no free ip address left in the pool finish here
               if( leaseObj == NULL ) 
               {
                  break;
               }
      
               // prepare reply
               dhcpMsg->op                   = 2;
               dhcpMsg->secs                 = 0;
               dhcpMsg->flags                = 0;
               *(uint32_t *)dhcpMsg->yiaddr  = *(uint32_t *)leaseObj->ip;
               memcpy(dhcpMsg->magic, magic_cookie, 4);
      
               // zero out options
               memset( dhcpMsg->options, 0x00, sizeof(dhcpMsg->options) );
      
               dhcpserver_fillOptions( dhcpMsg->options, DHCP_OFFER, dhcpconf->domain,
                  *(uint32_t*)dhcpconf->dns, leaseObj->leasetime, *(uint32_t*)dhcpconf->dhcpip,
                  *(uint32_t*)dhcpconf->dhcpip, *(uint32_t*)dhcpconf->sub );
               
               memcpy( pucTxRxBuffer, dhcpMsg, sizeof(DHCP_MSG_t));
               xDestinationAddress.sin_addr = FreeRTOS_inet_addr_quick( 255, 255, 255, 255 );
               xDestinationAddress.sin_port = FreeRTOS_htons( 68 );
               FreeRTOS_sendto( xListeningSocket, pucTxRxBuffer, sizeof(DHCP_MSG_t), 0, &xDestinationAddress, sizeof( xDestinationAddress ) );
               break;
      
            case DHCP_REQUEST:
               // is there a requested ip address in the option fields?
               ptr = dhcpserver_findOption( dhcpMsg->options, sizeof(dhcpMsg->options), DHCP_IPADDRESS);
               if( ptr == NULL )
               {
                  break;
               }
               if( ptr[1] != 4 )
               {
                  break;
               }
               ptr += 2;
      
               // is the mac address already in the dhcp lease table?
               leaseObj = dhcpserver_lookupMac( dhcpMsg->chaddr );
               if( leaseObj != NULL )
               {
                  // if yes delete
                  dhcpserver_lookupDelete( leaseObj );
               }
                  
               // check if requested ip address is available
               leaseObj = dhcpserver_lookupIp( *(uint32_t *)ptr ); 
               // if not available end here
               if( leaseObj == NULL )
               {
                  break;
               }
               
               // check if ip address is not in use
               if( dhcpserver_lookupFreeObj(leaseObj) != 1 )
               {
                  break;
               }
               
               // write mac address to the designated ip address
               dhcpserver_lookupSetMac( leaseObj, dhcpMsg->chaddr );
      
               // prepare dhcp response message
               memcpy( dhcpMsg->yiaddr, ptr, 4u );
               dhcpMsg->op = 2;
               dhcpMsg->secs = 0;
               dhcpMsg->flags = 2;
               memcpy(dhcpMsg->magic, magic_cookie, 4);
      
               // zero out options
               memset( dhcpMsg->options, 0x00, sizeof(dhcpMsg->options) );
  
               // fill options
               dhcpserver_fillOptions( dhcpMsg->options, DHCP_ACK, dhcpconf->domain,
                  *(uint32_t*)dhcpconf->dns, leaseObj->leasetime, *(uint32_t*)dhcpconf->dhcpip,
                  *(uint32_t*)dhcpconf->dhcpip, *(uint32_t*)dhcpconf->sub );
               
               // send acknowledge
               memcpy( pucTxRxBuffer, dhcpMsg, sizeof(DHCP_MSG_t));
               xDestinationAddress.sin_addr = FreeRTOS_inet_addr_quick( 255, 255, 255, 255 );
               xDestinationAddress.sin_port = FreeRTOS_htons( 68 );
               FreeRTOS_sendto( xListeningSocket, pucTxRxBuffer, sizeof(DHCP_MSG_t), 0, &xDestinationAddress, sizeof( xDestinationAddress ) );
               break;
      
            default:
                  break;
         }
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

// ----------------------------------------------------------------------------
/// \brief     Search for ip address and return dhcp table object.
///
/// \param     [in]  uint32_t ip
///
/// \return    pointer to leasetableObj_t
static leasetableObj_t *dhcpserver_lookupIp( uint32_t ip )
{
   if( dhcpconf == NULL )
   {
      return NULL;
   }
   
   for( uint16_t i=0; i<dhcpconf->tablelength; i++ )
   {
      if( *(uint32_t *)dhcpconf->leasetable[i].ip == ip )
      {
         return &dhcpconf->leasetable[i];
      }
   }
   return NULL;
}

// ----------------------------------------------------------------------------
/// \brief     Search for mac address and return dhcp table object.
///
/// \param     [in]  uint8_t *mac
///
/// \return    pointer to leasetableObj_t
static leasetableObj_t *dhcpserver_lookupMac( uint8_t *mac )
{
   if( dhcpconf == NULL )
   {
      return NULL;
   }
   
   for( uint16_t i=0; i<dhcpconf->tablelength; i++ )
   {
      if( memcmp( dhcpconf->leasetable[i].mac, mac, 6u ) == 0 )
      {
         return &dhcpconf->leasetable[i];
      }
   }
   return NULL;
}

// ----------------------------------------------------------------------------
/// \brief     Register mac address on the dhcp lease table.
///
/// \param     [in]  uint8_t *mac
/// \param     [in]  uint8_t *ip
///
/// \return    pointer to leasetableObj_t
static leasetableObj_t *dhcpserver_lookupRegister( uint8_t *mac, uint8_t *ip )
{
   if( dhcpconf == NULL )
   {
      return NULL;
   }
   
   for( uint16_t i=0; i<dhcpconf->tablelength; i++ )
   {
      if( memcmp( dhcpconf->leasetable[i].ip, ip, 4u ) == 0 )
      {
         memcpy( dhcpconf->leasetable[i].mac, mac, 6u );
         return &dhcpconf->leasetable[i];
      }
   }
   return NULL;
}

// ----------------------------------------------------------------------------
/// \brief     Search for a free ip address in the dhcp lease table
///
/// \param     none
///
/// \return    pointer to leasetableObj_t
static leasetableObj_t *dhcpserver_lookupFree( void )
{
   static const uint8_t empty[6] = {0,0,0,0,0,0};
   if( dhcpconf == NULL )
   {
      return NULL;
   }
   
   for( uint16_t i=0; i<dhcpconf->tablelength; i++ )
   {
      if( memcmp( dhcpconf->leasetable[i].mac, empty, 6u ) == 0 )
      {
         return &dhcpconf->leasetable[i];
      }
   }
   return NULL;
}

// ----------------------------------------------------------------------------
/// \brief     Delete mac address in the dhcp lease table.
///
/// \param     [in]  leasetableObj_t* tableObj
///
/// \return    void
static void dhcpserver_lookupDelete( leasetableObj_t* tableObj )
{
   if( dhcpconf == NULL )
   {
      return;
   }
   
   memset( tableObj->mac, 0x00, 6u );
}

// ----------------------------------------------------------------------------
/// \brief     Set mac address in the dhcp lease table.
///
/// \param     [in]  leasetableObj_t* tableObj
///
/// \return    void
static void dhcpserver_lookupSetMac( leasetableObj_t* tableObj, uint8_t* mac )
{
   if( dhcpconf == NULL )
   {
      return;
   }
   
   memcpy( tableObj->mac, mac, 6u );
}

// ----------------------------------------------------------------------------
/// \brief     Delete mac address in the dhcp lease table.
///
/// \param     [in]  leasetableObj_t* tableObj
///
/// \return    0 = error, 1 = free, 2 = not free
static uint8_t dhcpserver_lookupFreeObj( leasetableObj_t* tableObj )
{
   static const uint8_t empty[6] = {0,0,0,0,0,0};
   if( dhcpconf == NULL )
   {
      return 0;
   }
   
   if( memcmp( tableObj->mac, empty, 6u ) == 0 )
   {
      return 1;
   }
   return 2;
}

// ----------------------------------------------------------------------------
/// \brief     Fill out dhcp message option fields.
///
/// \param     [in]  void *dest
/// \param     [in]  uint8_t msg_type
/// \param     [in]  const char *domain
/// \param     [in]  uint32_t dns
/// \param     [in]  uint32_t lease_time
/// \param     [in]  uint32_t serverid
/// \param     [in]  uint32_t router
/// \param     [in]  uint32_t subnet
///
/// \return    0 = error, 1 = free, 2 = not free
static void dhcpserver_fillOptions( void *dest, uint8_t msg_type, const char *domain, uint32_t dns, uint32_t lease_time, uint32_t serverid, uint32_t router, uint32_t subnet )
{
	uint8_t *ptr = (uint8_t *)dest;
	/* ACK message type */
	*ptr++ = 53;
	*ptr++ = 1;
	*ptr++ = msg_type;

	/* dhcp server identifier */
	*ptr++ = DHCP_SERVERID;
	*ptr++ = 4;
	*(uint32_t *)ptr = serverid;
	ptr += 4;

	/* lease time */
	*ptr++ = DHCP_LEASETIME;
	*ptr++ = 4;
	*ptr++ = (lease_time >> 24) & 0xFF;
	*ptr++ = (lease_time >> 16) & 0xFF;
	*ptr++ = (lease_time >> 8) & 0xFF;
	*ptr++ = (lease_time >> 0) & 0xFF;

	/* subnet mask */
	*ptr++ = DHCP_SUBNETMASK;
	*ptr++ = 4;
	*(uint32_t *)ptr = subnet;
	ptr += 4;

	/* router */
	if (router != 0)
	{
		*ptr++ = DHCP_ROUTER;
		*ptr++ = 4;
		*(uint32_t *)ptr = router;
		ptr += 4;
	}

	/* domain name */
	if (domain != NULL)
	{
		int len = strlen(domain);
		*ptr++ = DHCP_DNSDOMAIN;
		*ptr++ = len;
		memcpy(ptr, domain, len);
		ptr += len;
	}

	/* domain name server (DNS) */
	if (dns != 0)
	{
		*ptr++ = DHCP_DNSSERVER;
		*ptr++ = 4;
		*(uint32_t *)ptr = dns;
		ptr += 4;
	}

	/* end */
	*ptr++ = DHCP_END;
}

// ----------------------------------------------------------------------------
/// \brief     Find dhcp option in a dhcp message.
///
/// \param     [in]  uint8_t *attrs
/// \param     [in]  uint16_t size
/// \param     [in]  uint8_t attr
///
/// \return    0 = error, 1 = free, 2 = not free
static uint8_t *dhcpserver_findOption( uint8_t *attrs, uint16_t size, uint8_t attr )
{
	uint16_t i = 0;
   uint16_t next;
	while ((i + 1) < size)
	{
		next = i + attrs[i + 1] + 2;
		if (next > size)
      {
         return NULL;
      }
		if (attrs[i] == attr)
      {
			return attrs + i;
      }
		i = next;
	}
	return NULL;
}

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/