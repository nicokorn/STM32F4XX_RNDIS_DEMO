// ****************************************************************************
/// \file      usbd_rndis.c
///
/// \brief     RNDIS device C source file
///
/// \details   The rndis usb device c source file contains the top level layer
///            of the usb device functionality. This library is based on the
///            irndis library from Sergey Fetisov: 
///            https://github.com/fetisov/lrndis
///            
///            The irindis libary is based on the older std peripheral library
///            from st. This is the HAL ported version of irndis with additional
///            code optimitation and performance improvements.
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

#include "usbd_rndis.h"
#include "main.h"
#include "usbd_ctlreq.h"
#include "queuex.h"
#include "ndis.h"
#include "rndis_protocol.h"
#include "usb_device.h"

// Private defines ************************************************************
#define ETH_HEADER_SIZE                   14
#define ETH_MAX_PACKET_SIZE               ETH_HEADER_SIZE + RNDIS_MTU
#define RNDIS_RX_BUFFER_SIZE              (ETH_MAX_PACKET_SIZE + sizeof(rndis_data_packet_t))

#define TX_STATE_READY                    0 /* initial transmitter state */
#define TX_STATE_NEED_SENDING             1 /* has user data to send */
#define TX_STATE_SENDING_HDR              2 /* sending first packet with header */
#define TX_STATE_SENDING_DATA             3 /* sending message data */
#define TX_STATE_SENDING_PADDING          4 /* sending one byte padding */
#define TX_STATE_RESET                    5 /* reset state */

#define USB_CONFIGURATION_DESCRIPTOR_TYPE 0x02
#define USB_INTERFACE_DESCRIPTOR_TYPE     0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE      0x05

#define INFBUF ((uint32_t *)((uint8_t *)&(m->RequestId) + m->InformationBufferOffset))

#define MAC_OPT NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA | \
			NDIS_MAC_OPTION_RECEIVE_SERIALIZED  | \
			NDIS_MAC_OPTION_TRANSFERS_NOT_PEND  | \
			NDIS_MAC_OPTION_NO_LOOPBACK

// Private types     **********************************************************

// Private variables **********************************************************
static usb_eth_stat_t         usb_eth_stat = {0};
static uint32_t               oid_packet_filter = 0x0000000;
static __ALIGN_BEGIN char*    rndis_rx_buffer __ALIGN_END;
static rndis_state_t          rndis_state;
static const uint8_t          station_hwaddr[6] = { RNDIS_HWADDR };
static const uint8_t          permanent_hwaddr[6] = { RNDIS_HWADDR };

// struct for the rndis transmission information and status
static struct
{
	uint8_t  *ptr;
	uint16_t size;
	uint16_t state;
	bool need_padding;
} tx =
{
	NULL,
	0,
	TX_STATE_RESET,
	false
};

// USB standard device descriptor
__ALIGN_BEGIN static uint8_t USBD_RNDIS_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

// USB device configuration descriptor
__ALIGN_BEGIN static uint8_t USBD_RNDIS_CfgDesc[] __ALIGN_END =
{
    /* Configuration descriptor */

    9,                                 /* bLength         = 9 bytes. */
    USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType = CONFIGURATION */
    0xDE, 0xAD,                        /* wTotalLength    = sizeof(USBD_RNDIS_CfgDesc) */
    0x02,                              /* bNumInterfaces  = 2 (RNDIS spec). */
    0x01,                              /* bConfValue      = 1 */
    0x00,                              /* iConfiguration  = unused. */
    0x40,                              /* bmAttributes    = Self-Powered. */
    0x01,                              /* MaxPower        = x2mA */

    /* IAD descriptor */

    0x08, /* bLength */
    0x0B, /* bDescriptorType */
    0x00, /* bFirstInterface */
    0x02, /* bInterfaceCount */
    0xE0, /* bFunctionClass (Wireless Controller) */
    0x01, /* bFunctionSubClass */
    0x03, /* bFunctionProtocol */
    0x00, /* iFunction */

    /* Interface 0 descriptor */
    
    9,                             /* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType = INTERFACE */
    0x00,                          /* bInterfaceNumber */
    0x00,                          /* bAlternateSetting */
    1,                             /* bNumEndpoints */
    0xE0,                          /* bInterfaceClass: Wireless Controller */
    0x01,                          /* bInterfaceSubClass */
    0x03,                          /* bInterfaceProtocol */
    0,                             /* iInterface */

    /* Interface 0 functional descriptor */

    /* Header Functional Descriptor */
    0x05, /* bFunctionLength */
    0x24, /* bDescriptorType = CS Interface */
    0x00, /* bDescriptorSubtype */
    0x10, /* bcdCDC = 1.10 */
    0x01, /* bcdCDC = 1.10 */

    /* Call Management Functional Descriptor */
    0x05, /* bFunctionLength */
    0x24, /* bDescriptorType = CS Interface */
    0x01, /* bDescriptorSubtype = Call Management */
    0x00, /* bmCapabilities */
    0x01, /* bDataInterface */

    /* Abstract Control Management Functional Descriptor */
    0x04, /* bFunctionLength */
    0x24, /* bDescriptorType = CS Interface */
    0x02, /* bDescriptorSubtype = Abstract Control Management */
    0x00, /* bmCapabilities = Device supports the notification Network_Connection */

    /* Union Functional Descriptor */
    0x05, /* bFunctionLength */
    0x24, /* bDescriptorType = CS Interface */
    0x06, /* bDescriptorSubtype = Union */
    0x00, /* bControlInterface = "RNDIS Communications Control" */
    0x01, /* bSubordinateInterface0 = "RNDIS Ethernet Data" */

    /* Endpoint descriptors for Communication Class Interface */

    7,                            /* bLength         = 7 bytes */
    USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType = ENDPOINT */
    RNDIS_NOTIFICATION_IN_EP,     /* bEndpointAddr   = IN - EP3 */
    0x03,                         /* bmAttributes    = Interrupt endpoint */
    8, 0,                         /* wMaxPacketSize */
    0x01,                         /* bInterval       = 1 ms polling from host */

    /* Interface 1 descriptor */

    9,                             /* bLength */
    USB_INTERFACE_DESCRIPTOR_TYPE, /* bDescriptorType */
    0x01,                          /* bInterfaceNumber */
    0x00,                          /* bAlternateSetting */
    2,                             /* bNumEndpoints */
    0x0A,                          /* bInterfaceClass: CDC */
    0x00,                          /* bInterfaceSubClass */
    0x00,                          /* bInterfaceProtocol */
    0x00,                          /* uint8  iInterface */

    /* Endpoint descriptors for Data Class Interface */

    7,                            /* bLength         = 7 bytes */
    USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType = ENDPOINT [IN] */
    RNDIS_DATA_IN_EP,             /* bEndpointAddr   = IN EP */
    0x02,                         /* bmAttributes    = BULK */
    RNDIS_DATA_IN_SZ, 0,          /* wMaxPacketSize */
    0,                            /* bInterval       = ignored for BULK */

    7,                            /* bLength         = 7 bytes */
    USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType = ENDPOINT [OUT] */
    RNDIS_DATA_OUT_EP,            /* bEndpointAddr   = OUT EP */
    0x02,                         /* bmAttributes    = BULK */
    RNDIS_DATA_OUT_SZ, 0,         /* wMaxPacketSize */
    0                             /* bInterval       = ignored for BULK */
};

// RNDIS options list
const uint32_t OIDSupportedList[] = 
{
    OID_GEN_SUPPORTED_LIST,
    OID_GEN_HARDWARE_STATUS,
    OID_GEN_MEDIA_SUPPORTED,
    OID_GEN_MEDIA_IN_USE,
    OID_GEN_MAXIMUM_FRAME_SIZE,
    OID_GEN_LINK_SPEED,
    OID_GEN_TRANSMIT_BLOCK_SIZE,
    OID_GEN_RECEIVE_BLOCK_SIZE,
    OID_GEN_VENDOR_ID,
    OID_GEN_VENDOR_DESCRIPTION,
    OID_GEN_VENDOR_DRIVER_VERSION,
    OID_GEN_CURRENT_PACKET_FILTER,
    OID_GEN_MAXIMUM_TOTAL_SIZE,
    OID_GEN_PROTOCOL_OPTIONS,
    OID_GEN_MAC_OPTIONS,
    OID_GEN_MEDIA_CONNECT_STATUS,
    OID_GEN_MAXIMUM_SEND_PACKETS,
    OID_802_3_PERMANENT_ADDRESS,
    OID_802_3_CURRENT_ADDRESS,
    OID_802_3_MULTICAST_LIST,
    OID_802_3_MAXIMUM_LIST_SIZE,
    OID_802_3_MAC_OPTIONS
};
#define OID_LIST_LENGTH (sizeof(OIDSupportedList) / sizeof(*OIDSupportedList))
#define ENC_BUF_SIZE    (OID_LIST_LENGTH * 4 + 32)
static uint8_t encapsulated_buffer[ENC_BUF_SIZE];

// Global variables ***********************************************************
extern USBD_HandleTypeDef  hUsbDeviceFS;
extern queue_handle_t      usbQueue;
extern queue_handle_t      tcpQueue;

// Private function prototypes ************************************************
static uint8_t    USBD_RNDIS_Init                           ( USBD_HandleTypeDef *pdev, uint8_t cfgidx );
static uint8_t    USBD_RNDIS_DeInit                         ( USBD_HandleTypeDef *pdev, uint8_t cfgidx );
static uint8_t    USBD_RNDIS_Setup                          ( USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req );
static uint8_t    USBD_RNDIS_EP0_RxReady                    ( USBD_HandleTypeDef *pdev );
static uint8_t    USBD_RNDIS_DataIn                         ( USBD_HandleTypeDef *pdev, uint8_t epnum );
static uint8_t    USBD_RNDIS_DataOut                        ( USBD_HandleTypeDef *pdev, uint8_t epnum );
static uint8_t    *USBD_RNDIS_GetFSCfgDesc                  ( uint16_t *length );
static uint8_t    *USBD_RNDIS_GetHSCfgDesc                  ( uint16_t *length );
static uint8_t    *USBD_RNDIS_GetOtherSpeedCfgDesc          ( uint16_t *length );
static uint8_t    *USBD_RNDIS_GetOtherSpeedCfgDesc          ( uint16_t *length );
static uint8_t    *USBD_RNDIS_GetDeviceQualifierDescriptor  ( uint16_t *length );
static void       USBD_RNDIS_query                          ( void *pdev );
static void       USBD_RNDIS_handleSetMsg                   ( void *pdev );
static void       USBD_RNDIS_handleConfigParm               ( const char *data, uint16_t keyoffset, uint16_t valoffset, uint16_t keylen, uint16_t vallen );
static void       USBD_RNDIS_packetFilter                   ( uint32_t newfilter );
static void       USBD_RNDIS_query_cmplt                    ( uint32_t status, const void *data, uint16_t size );

// RNDIS interface class callbacks structure
USBD_ClassTypeDef USBD_RDNIS =
{
  USBD_RNDIS_Init,
  USBD_RNDIS_DeInit,
  USBD_RNDIS_Setup,
  NULL,                 // EP0_TxSent
  USBD_RNDIS_EP0_RxReady,
  USBD_RNDIS_DataIn,
  USBD_RNDIS_DataOut,
  NULL,
  NULL,
  NULL,
  USBD_RNDIS_GetHSCfgDesc,
  USBD_RNDIS_GetFSCfgDesc,
  USBD_RNDIS_GetOtherSpeedCfgDesc,
  USBD_RNDIS_GetDeviceQualifierDescriptor,
};

//------------------------------------------------------------------------------
/// \brief     Returns rndis class struct.      
///
/// \param     none
///
/// \return    USBD_ClassTypeDef*
USBD_ClassTypeDef* USBD_RNDIS_getClass( void )
{
   return &USBD_RDNIS;
}

//------------------------------------------------------------------------------
/// \brief     Rndis init function. Called by the usb stack.      
///
/// \param     [in/out] USBD_HandleTypeDef *pdev
/// \param     [in]     uint8_t cfgidx (unused)
///
/// \return    init status
static uint8_t USBD_RNDIS_Init( USBD_HandleTypeDef *pdev, uint8_t cfgidx )
{
   UNUSED(cfgidx);
   (void)rndis_state; // opress unsued compiler warning
   
   // Open Command IN EP, this is the interrupt in endpoint used for communication
   // on the control endpoints. The control endpoints are initialized not here.
   USBD_LL_OpenEP( pdev, RNDIS_NOTIFICATION_IN_EP, USBD_EP_TYPE_INTR, RNDIS_NOTIFICATION_IN_SZ );
  
   // Open EP IN 
   USBD_LL_OpenEP( pdev, RNDIS_DATA_IN_EP, USBD_EP_TYPE_BULK, RNDIS_DATA_IN_SZ );
   
   // Open EP OUT
   USBD_LL_OpenEP( pdev, RNDIS_DATA_OUT_EP, USBD_EP_TYPE_BULK, RNDIS_DATA_OUT_SZ );
   
   // Set the data receive pointer.
   rndis_rx_buffer = (char*)queue_getHeadBuffer( &usbQueue );
   
   // Prepare Out endpoint to receive next packet
   USBD_LL_PrepareReceive( pdev, RNDIS_DATA_OUT_EP, (uint8_t*)rndis_rx_buffer, QUEUEBUFFERLENGTH );
   
   // set rndis state to ready
   tx.state = TX_STATE_READY;
   
   // init the queue
   queue_init(&tcpQueue);

   return USBD_OK;
}

//------------------------------------------------------------------------------
/// \brief     Rndis deinit function. Called by the usb stack.      
///
/// \param     [in/out] USBD_HandleTypeDef *pdev
/// \param     [in]     uint8_t cfgidx (unused)
///
/// \return    status
static uint8_t USBD_RNDIS_DeInit( USBD_HandleTypeDef *pdev, uint8_t cfgidx )
{
   // Close notification endpoint
   USBD_LL_CloseEP( pdev, RNDIS_NOTIFICATION_IN_EP );
   
   // Close data in endpoint
   USBD_LL_CloseEP( pdev, RNDIS_DATA_IN_EP );
   
   // close data out endpoint
   USBD_LL_CloseEP( pdev, RNDIS_DATA_OUT_EP );
   
   // set transmission state to reset
   tx.state = TX_STATE_RESET;
   
   return USBD_OK;
}

//------------------------------------------------------------------------------
/// \brief     Rndis setup function. Called by the usb stack.      
///
/// \param     [in/out] USBD_HandleTypeDef *pdev
/// \param     [in]     uint8_t cfgidx (unused)
///
/// \return    status
static uint8_t USBD_RNDIS_Setup( USBD_HandleTypeDef  *pdev, USBD_SetupReqTypedef *req )
{
   switch ( req->bmRequest & USB_REQ_TYPE_MASK )
   {
      case USB_REQ_TYPE_CLASS :
         if (req->wLength != 0) // Is it a data setup packet?
         {
            // Check if the request is Device-to-Host
            if (req->bmRequest & 0x80)
            {
               USBD_CtlSendData( pdev, encapsulated_buffer, ((rndis_generic_msg_t *)encapsulated_buffer)->MessageLength );
            }
            else // Host-to-Device requeset
            {
               USBD_CtlPrepareRx( pdev, encapsulated_buffer, req->wLength );          
            }
         }  
         return USBD_OK;
      default:
         return USBD_OK;
   }
}

//------------------------------------------------------------------------------
/// \brief     Endpoint 0 (ctrl endpoint) ready function called by the usb 
///            stack.      
///
/// \param     [in/out] USBD_HandleTypeDef *pdev
///
/// \return    status
static uint8_t USBD_RNDIS_EP0_RxReady( USBD_HandleTypeDef *pdev )
{
   switch (((rndis_generic_msg_t *)encapsulated_buffer)->MessageType)
   {
      case REMOTE_NDIS_INITIALIZE_MSG:
         {
            rndis_initialize_cmplt_t *m;
            m = ((rndis_initialize_cmplt_t *)encapsulated_buffer);
            // m->MessageID is same as before
            m->MessageType = REMOTE_NDIS_INITIALIZE_CMPLT;
            m->MessageLength = sizeof(rndis_initialize_cmplt_t);
            m->MajorVersion = RNDIS_MAJOR_VERSION;
            m->MinorVersion = RNDIS_MINOR_VERSION;
            m->Status = RNDIS_STATUS_SUCCESS;
            m->DeviceFlags = RNDIS_DF_CONNECTIONLESS;
            m->Medium = RNDIS_MEDIUM_802_3;
            m->MaxPacketsPerTransfer = 1;
            m->MaxTransferSize = RNDIS_RX_BUFFER_SIZE;
            m->PacketAlignmentFactor = 0;
            m->AfListOffset = 0;
            m->AfListSize = 0;
            rndis_state = rndis_initialized;
            USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_NOTIFICATION_IN_EP, (uint8_t *)"\x01\x00\x00\x00\x00\x00\x00\x00", 8);
         }
         break;
   
      case REMOTE_NDIS_QUERY_MSG:
         USBD_RNDIS_query(pdev);
         break;
         
      case REMOTE_NDIS_SET_MSG:
         USBD_RNDIS_handleSetMsg(pdev);
         break;
   
      case REMOTE_NDIS_RESET_MSG:
         {
            rndis_reset_cmplt_t * m;
            m = ((rndis_reset_cmplt_t *)encapsulated_buffer);
            rndis_state = rndis_uninitialized;
            m->MessageType = REMOTE_NDIS_RESET_CMPLT;
            m->MessageLength = sizeof(rndis_reset_cmplt_t);
            m->Status = RNDIS_STATUS_SUCCESS;
            m->AddressingReset = 1; // Make it look like we did something
            // m->AddressingReset = 0; - Windows halts if set to 1 for some reason
            USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_NOTIFICATION_IN_EP, (uint8_t *)"\x01\x00\x00\x00\x00\x00\x00\x00", 8);
         }
         break;
   
      case REMOTE_NDIS_KEEPALIVE_MSG:
         {
            rndis_keepalive_cmplt_t * m;
            m = (rndis_keepalive_cmplt_t *)encapsulated_buffer;
            m->MessageType = REMOTE_NDIS_KEEPALIVE_CMPLT;
            m->MessageLength = sizeof(rndis_keepalive_cmplt_t);
            m->Status = RNDIS_STATUS_SUCCESS;
         }
         // We have data to send back
         USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_NOTIFICATION_IN_EP, (uint8_t *)"\x01\x00\x00\x00\x00\x00\x00\x00", 8);
         break;
   
      default:
         break;
   }
   return USBD_OK;
}

//------------------------------------------------------------------------------
/// \brief     Data input function called by the usb stack.
///
/// \param     [in/out] USBD_HandleTypeDef *pdev
/// \param     [in]     uint8_t epnum
///
/// \return    status
static uint8_t USBD_RNDIS_DataIn( USBD_HandleTypeDef *pdev, uint8_t epnum )
{
   UNUSED(pdev);
   
	epnum &= 0x0F;
	if( epnum == (RNDIS_DATA_IN_EP & 0x0F) )
	{
		if( tx.state == TX_STATE_SENDING_DATA )
		{
			if( tx.need_padding )
			{
				USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_DATA_IN_EP, (uint8_t *)"\0", 1);
				tx.state = TX_STATE_SENDING_PADDING;
				return USBD_OK;
			}
			tx.state = TX_STATE_READY;
         on_usbInTxCplt();
			return USBD_OK;
		}
		
		if( tx.state == TX_STATE_SENDING_PADDING )
		{
			tx.state = TX_STATE_READY;
         on_usbInTxCplt();
			return USBD_OK;
		}
	}
	return USBD_OK;
}

//------------------------------------------------------------------------------
/// \brief     Handled packet function.
///
/// \param     [in]  const char *data
/// \param     [in]  uint16_t size
///
/// \return    none
static void USBD_RNDIS_handlePacket(const char *data, uint16_t size)
{
	rndis_data_packet_t *p;
	p = (rndis_data_packet_t *)data;
	if (size < sizeof(rndis_data_packet_t) || size > QUEUEBUFFERLENGTH)
   {
		usb_eth_stat.rxbad++;
		return;
   }
	if (p->MessageType != REMOTE_NDIS_PACKET_MSG)
   {
		usb_eth_stat.rxbad++;
		return;
   }
	if (p->DataOffset + offsetof(rndis_data_packet_t, DataOffset) + p->DataLength != size)
	{
      // padding?
      if( p->MessageLength != size-1u )
      {
         usb_eth_stat.rxbad++;
         return;
      }
	}
	usb_eth_stat.rxok++;
   on_usbOutRxPacket( &rndis_rx_buffer[p->DataOffset + offsetof(rndis_data_packet_t, DataOffset)], p->DataLength );
}

//------------------------------------------------------------------------------
/// \brief     Data received on non-control Out endpoint called by the usb
///            stack.
///
/// \param     [in/out] USBD_HandleTypeDef *pdev
/// \param     [in]     uint8_t epnum
///
/// \return    status
static uint8_t USBD_RNDIS_DataOut( USBD_HandleTypeDef *pdev, uint8_t epnum )
{
   UNUSED(pdev);
   
	if( epnum == RNDIS_DATA_OUT_EP )
	{  
      PCD_EPTypeDef *ep = &((PCD_HandleTypeDef*)pdev->pData)->OUT_ep[epnum]; 
      USBD_RNDIS_handlePacket(rndis_rx_buffer, ep->xfer_count);
		USBD_LL_PrepareReceive(&hUsbDeviceFS, RNDIS_DATA_OUT_EP, (uint8_t*)(rndis_rx_buffer), QUEUEBUFFERLENGTH);
	}
   return USBD_OK;
}

//------------------------------------------------------------------------------
/// \brief     Returns if rndis usb is ready to send next packet.
///
/// \param     none
///
/// \return    bool
bool USBD_RNDIS_canSend(void)
{
	return tx.state == TX_STATE_READY;
}

//------------------------------------------------------------------------------
/// \brief     Requests to send next packet over rndis usb.
///
/// \param     [in]  const void *data
/// \param     [in]  uint16_t size
///
/// \return    bool
bool USBD_RNDIS_send( const void *data, uint16_t size )
{
	if( tx.state != TX_STATE_READY )
   {
      return false;
   }
   if( size > ETH_MAX_PACKET_SIZE || size == 0 )
   {
      return false;
   }

	__disable_irq();
   
   tx.ptr = (uint8_t *)data-44u;    // there is allocated memory in front of data for the usb header
	tx.size = size+44u;              // add 44 byte of header for the complete length
	tx.state = TX_STATE_NEED_SENDING;

   rndis_data_packet_t *hdr;
   hdr = (rndis_data_packet_t *)tx.ptr;
   memset(hdr, 0, sizeof(rndis_data_packet_t));
   hdr->MessageType     = REMOTE_NDIS_PACKET_MSG;
   hdr->MessageLength   = sizeof(rndis_data_packet_t) + tx.size-44u; // substract header size
   hdr->DataOffset      = sizeof(rndis_data_packet_t) - offsetof(rndis_data_packet_t, DataOffset);
   hdr->DataLength      = tx.size-44u; // substract header size
   
   tx.need_padding = (hdr->MessageLength & (RNDIS_DATA_IN_SZ - 1)) == 0;
   if (tx.need_padding)
   {
      hdr->MessageLength++;
   }
   
   USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_DATA_IN_EP, tx.ptr, (uint32_t)tx.size);
   tx.state = TX_STATE_SENDING_DATA;

	__enable_irq();

	return true;
}

//------------------------------------------------------------------------------
/// \brief     Setting buffer for receiving next frame.
///
/// \param     [in]  uint8_t* buffer
///
/// \return    none
void USBD_RNDIS_setBuffer( uint8_t* buffer )
{
   rndis_rx_buffer = (char*)buffer;
}

//------------------------------------------------------------------------------
/// \brief     USBD_CDC_GetFSCfgDesc Return configuration descriptor.
///
/// \param     [in]  uint16_t *length
///
/// \return    pointer to descriptor buffer
static uint8_t *USBD_RNDIS_GetFSCfgDesc( uint16_t *length )
{
   *length = (uint16_t)sizeof(USBD_RNDIS_CfgDesc);
   USBD_RNDIS_CfgDesc[2] = sizeof(USBD_RNDIS_CfgDesc) & 0xFF;
   USBD_RNDIS_CfgDesc[3] = (sizeof(USBD_RNDIS_CfgDesc) >> 8) & 0xFF;
   return USBD_RNDIS_CfgDesc;
}

//------------------------------------------------------------------------------
/// \brief     USBD_CDC_GetHSCfgDesc Return configuration descriptor.
///
/// \param     [in]  uint16_t *length
///
/// \return    pointer to descriptor buffer
static uint8_t *USBD_RNDIS_GetHSCfgDesc( uint16_t *length )
{
  return NULL;
}

//------------------------------------------------------------------------------
/// \brief     USBD_CDC_GetOtherSpeedCfgDesc Return configuration descriptor.
///
/// \param     [in]  uint16_t *length
///
/// \return    pointer to descriptor buffer
static uint8_t *USBD_RNDIS_GetOtherSpeedCfgDesc( uint16_t *length )
{
  return NULL;
}

//------------------------------------------------------------------------------
/// \brief     USBD_CDC_GetDeviceQualifierDescriptor Return configuration descriptor.
///
/// \param     [in]  uint16_t *length
///
/// \return    pointer to descriptor buffer
static uint8_t *USBD_RNDIS_GetDeviceQualifierDescriptor(uint16_t *length)
{
  *length = (uint16_t)sizeof(USBD_RNDIS_DeviceQualifierDesc);

  return USBD_RNDIS_DeviceQualifierDesc;
}

//------------------------------------------------------------------------------
/// \brief     USBD_RNDIS_RegisterInterface
///
/// \param     [in]  USBD_HandleTypeDef *pdev
/// \param     [in]  USBD_RNDIS_ItfTypeDef *fops
///
/// \return    status
uint8_t USBD_RNDIS_RegisterInterface( USBD_HandleTypeDef *pdev, USBD_RNDIS_ItfTypeDef *fops )
{
  if (fops == NULL)
  {
    return (uint8_t)USBD_FAIL;
  }

  pdev->pUserData = fops;

  return (uint8_t)USBD_OK;
}

//------------------------------------------------------------------------------
/// \brief     Query response function 32 bit.
///
/// \param     [in]  uint32_t status
/// \param     [in]  uint32_t data
///
/// \return    none
void USBD_RNDIS_query_cmplt32( uint32_t status, uint32_t data )
{
   rndis_query_cmplt_t *c;
   c = (rndis_query_cmplt_t *)encapsulated_buffer;
   c->MessageType = REMOTE_NDIS_QUERY_CMPLT;
   c->MessageLength = sizeof(rndis_query_cmplt_t) + 4;
   c->InformationBufferLength = 4;
   c->InformationBufferOffset = 16;
   c->Status = status;
   *(uint32_t *)(c + 1) = data;
   USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_NOTIFICATION_IN_EP, (uint8_t *)"\x01\x00\x00\x00\x00\x00\x00\x00", 8);
}

//------------------------------------------------------------------------------
/// \brief     Query response function.
///
/// \param     [in]  uint32_t status
/// \param     [in]  const void *data
/// \param     [in]  uint16_t size
///
/// \return    none
static void USBD_RNDIS_query_cmplt( uint32_t status, const void *data, uint16_t size )
{
	rndis_query_cmplt_t *c;
	c = (rndis_query_cmplt_t *)encapsulated_buffer;
	c->MessageType = REMOTE_NDIS_QUERY_CMPLT;
	c->MessageLength = sizeof(rndis_query_cmplt_t) + size;
	c->InformationBufferLength = size;
	c->InformationBufferOffset = 16;
	c->Status = status;
	memcpy(c + 1, data, size);
	USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_NOTIFICATION_IN_EP, (uint8_t *)"\x01\x00\x00\x00\x00\x00\x00\x00", 8);
}

//------------------------------------------------------------------------------
/// \brief     Query response parser function.
///
/// \param     [in]  void *pdev
///
/// \return    none
static void USBD_RNDIS_query( void *pdev )
{
	switch (((rndis_query_msg_t *)encapsulated_buffer)->Oid)
	{
		case OID_GEN_SUPPORTED_LIST:         USBD_RNDIS_query_cmplt(RNDIS_STATUS_SUCCESS, OIDSupportedList, 4 * OID_LIST_LENGTH); return;
		case OID_GEN_VENDOR_DRIVER_VERSION:  USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0x00001000);  return;
		case OID_802_3_CURRENT_ADDRESS:      USBD_RNDIS_query_cmplt(RNDIS_STATUS_SUCCESS, &station_hwaddr, 6); return;
		case OID_802_3_PERMANENT_ADDRESS:    USBD_RNDIS_query_cmplt(RNDIS_STATUS_SUCCESS, &permanent_hwaddr, 6); return;
		case OID_GEN_MEDIA_SUPPORTED:        USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3); return;
		case OID_GEN_MEDIA_IN_USE:           USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3); return;
		case OID_GEN_PHYSICAL_MEDIUM:        USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIUM_802_3); return;
		case OID_GEN_HARDWARE_STATUS:        USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0); return;
		case OID_GEN_LINK_SPEED:             USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, RNDIS_LINK_SPEED / 100); return;
		case OID_GEN_VENDOR_ID:              USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0x00FFFFFF); return;
		case OID_GEN_VENDOR_DESCRIPTION:     USBD_RNDIS_query_cmplt(RNDIS_STATUS_SUCCESS, RNDIS_VENDOR, strlen(RNDIS_VENDOR) + 1); return;
		case OID_GEN_CURRENT_PACKET_FILTER:  USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, oid_packet_filter); return;
		case OID_GEN_MAXIMUM_FRAME_SIZE:     USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE - ETH_HEADER_SIZE); return;
		case OID_GEN_MAXIMUM_TOTAL_SIZE:     USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE); return;
		case OID_GEN_TRANSMIT_BLOCK_SIZE:    USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE); return;
		case OID_GEN_RECEIVE_BLOCK_SIZE:     USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, ETH_MAX_PACKET_SIZE); return;
		case OID_GEN_MEDIA_CONNECT_STATUS:   USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, NDIS_MEDIA_STATE_CONNECTED); return;
		case OID_GEN_RNDIS_CONFIG_PARAMETER: USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0); return;
		case OID_802_3_MAXIMUM_LIST_SIZE:    USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 1); return;
		case OID_802_3_MULTICAST_LIST:       USBD_RNDIS_query_cmplt32(RNDIS_STATUS_NOT_SUPPORTED, 0); return;
		case OID_802_3_MAC_OPTIONS:          USBD_RNDIS_query_cmplt32(RNDIS_STATUS_NOT_SUPPORTED, 0); return;
		case OID_GEN_MAC_OPTIONS:            USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, /*MAC_OPT*/ 0); return;
		case OID_802_3_RCV_ERROR_ALIGNMENT:  USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0); return;
		case OID_802_3_XMIT_ONE_COLLISION:   USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0); return;
		case OID_802_3_XMIT_MORE_COLLISIONS: USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0); return;
		case OID_GEN_XMIT_OK:                USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.txok); return;
		case OID_GEN_RCV_OK:                 USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.rxok); return;
		case OID_GEN_RCV_ERROR:              USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.rxbad); return;
		case OID_GEN_XMIT_ERROR:             USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, usb_eth_stat.txbad); return;
		case OID_GEN_RCV_NO_BUFFER:          USBD_RNDIS_query_cmplt32(RNDIS_STATUS_SUCCESS, 0); return;
		default:                             USBD_RNDIS_query_cmplt(RNDIS_STATUS_FAILURE, NULL, 0); return;
	}
}

//------------------------------------------------------------------------------
/// \brief     Config parameter handler
///
/// \param     [in]  const char *data
/// \param     [in]  uint16_t keyoffset
/// \param     [in]  uint16_t valoffset
/// \param     [in]  uint16_t keylen
/// \param     [in]  uint16_t vallen
///
/// \return    none
static void USBD_RNDIS_handleConfigParm( const char *data, uint16_t keyoffset, uint16_t valoffset, uint16_t keylen, uint16_t vallen )
{
    (void)data;
    (void)keyoffset;
    (void)valoffset;
    (void)keylen;
    (void)vallen;
}

//------------------------------------------------------------------------------
/// \brief     Packetfilter function.
///
/// \param     [in]  uint32_t newfilter
///
/// \return    none
static void USBD_RNDIS_packetFilter( uint32_t newfilter )
{
    (void)newfilter;
}

//------------------------------------------------------------------------------
/// \brief     Rndis message builder function.
///
/// \param     [in]  void *pdev
///
/// \return    none
static void USBD_RNDIS_handleSetMsg( void *pdev )
{
	rndis_set_cmplt_t *c;
	rndis_set_msg_t *m;
	rndis_Oid_t oid;

	c = (rndis_set_cmplt_t *)encapsulated_buffer;
	m = (rndis_set_msg_t *)encapsulated_buffer;

	oid = m->Oid;
	c->MessageType = REMOTE_NDIS_SET_CMPLT;
	c->MessageLength = sizeof(rndis_set_cmplt_t);
	c->Status = RNDIS_STATUS_SUCCESS;

	switch (oid)
	{
		// Parameters set up in 'Advanced' tab
		case OID_GEN_RNDIS_CONFIG_PARAMETER:
			{
                rndis_config_parameter_t *p;
				char *ptr = (char *)m;
				ptr += sizeof(rndis_generic_msg_t);
				ptr += m->InformationBufferOffset;
				p = (rndis_config_parameter_t *)ptr;
				USBD_RNDIS_handleConfigParm(ptr, p->ParameterNameOffset, p->ParameterValueOffset, p->ParameterNameLength, p->ParameterValueLength);
			}
			break;

		// Mandatory general OIDs
		case OID_GEN_CURRENT_PACKET_FILTER:
			oid_packet_filter = *INFBUF;
			if (oid_packet_filter)
			{
				USBD_RNDIS_packetFilter(oid_packet_filter);
				rndis_state = rndis_data_initialized;
			} 
			else 
			{
				rndis_state = rndis_initialized;
			}
			break;

		case OID_GEN_CURRENT_LOOKAHEAD:
			break;

		case OID_GEN_PROTOCOL_OPTIONS:
			break;

		// Mandatory 802_3 OIDs
		case OID_802_3_MULTICAST_LIST:
			break;

		// Power Managment: fails for now
		case OID_PNP_ADD_WAKE_UP_PATTERN:
		case OID_PNP_REMOVE_WAKE_UP_PATTERN:
		case OID_PNP_ENABLE_WAKE_UP:
		default:
			c->Status = RNDIS_STATUS_FAILURE;
			break;
	}

	// c->MessageID is same as before
	USBD_LL_Transmit(&hUsbDeviceFS, RNDIS_NOTIFICATION_IN_EP, (uint8_t *)"\x01\x00\x00\x00\x00\x00\x00\x00", 8);
	return;
}

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/