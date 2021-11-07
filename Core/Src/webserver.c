// ****************************************************************************
/// \file      webserver.c
///
/// \brief     web server module
///
/// \details   Handles webserver requests from clients.
///
/// \author    Nico Korn
///
/// \version   0.3.0.0
///
/// \date      07112021
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
#include "webserver.h"
#include "led.h"
#include "monitor.h"
#include "printf.h"

#include "cmsis_os.h"
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_DHCP.h"

// Private defines ************************************************************
#define URIBUFFER       ( 500u )
#define TXBUFFERSIZE    ( 4000u )

#define RMLOGO          "<img src='data:image/png;base64,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'/>"
#define FAVICON         "<link rel=\"shortcut icon\" href=\"data:image/gif;base64,R0lGODlhIAAgAHAAACwAAAAAIAAgAIf////s2+Pqt8j/wNT/v836wsv/xM7/wM7/w9L6ws38uMP4ZHTQCyfgAB7cABnXBhzeBh7bABjiAR/iAB3UAC3wdYfZAwvdAybkAyHqBivjABrgAyblASbiASHjAiLkACXwZ4XkABHsBDTgBCjVASnfASbgBS3eACnhACjiASnXBR7tfpHdAxHpAC3sAifiACXtACXiACbfACXjASfjABnmY3XbBgrYAB7qAB3tAB3tABrgAB3iAB7hAB7ZCCX31+z7vbLjycztw8T0v8fxw8Puysr3vsf1wMj/s6/6v93+m57oo7Xxnaz/nrX3m6rworD9mbH5nLH6nbL1pKPhaXLbBQPfABvhABDrAx3pABDeABDjABbfABXgARbdASXgfY/iABD9AjfcACLTByjoByfgAiffAyfkABvya4ngABDvAzPUAyLfASjjAiTlACndASfeACXkARzleIvZBRHqACvhACLoACjeASDnASjdBCHpaH7gDhfkAyvkBSLqACfeBSDgBybnBSrkBCbjACn67//3x7vz2N354Nz/0+D62t310tj72dr62Nn9zcH4rc7zgobqjKTskJ/6gaDvjqLsiKDwip/viZ7lhYnuanjjDhDjACDhABfmABjdARrnAh/hABniABruAS/ddILTBAzqACjiBiLsCC3hASPkAyPlBCTYAh72b4/iABXwBDXUAyPeACvlACvdACvdACbeACfgAinhBR7vdXLVABnnABvSARflBSDdBB/mAB/vaHnmEz70FULfFzvsFD3lFD7pCzrnEDrVFDMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAI/wABCBxIsKDBgwgTKlzIsKHDhxAjPvwhABEBAgYIWDTAsWPGhwEcXeR4caQAjCcVPKyRS4uWllrEuJQpE4aWNQ8rxBSjRowYGD6BAv0p5iEILT19xopFtKnPh5a0wOgJYyhPnz3VaFlZZU2WqkFpii3q8IeCIEWKFCoiwMDJIigJAHmYRMkTJk2gPIHCly9evlMeUqniMksWwi4TJybbUJSWMGLWXFUqhinTrQ6PqlkTayhTykEfOhaK9SfVqU8zZ2I6VM1Qq0AhFklEKFGiQwFu07Ztm9FDRxUsWZIECZJw4Y+OW7L1sBfiLC5tztQCHanoUVl9EgUrNJdRpKU/Ky5dyvShLTFZcml/7TPLT8wNexX7JezXfDXFiOknZp9YMYkABijggAQWaOCBAAQEADs=\" />"

// Private types     **********************************************************

// Private variables **********************************************************
osThreadId_t webserverListenTaskToNotify;
const osThreadAttr_t webserverListenTask_attributes = {
  .name = "HTTPListen-task",
  .stack_size = configMINIMAL_STACK_SIZE * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

osThreadId_t webserverHandleTaskToNotify;
const osThreadAttr_t webserverHandleTask_attributes = {
  .name = "HTTP-task",
  .stack_size = 4 * configMINIMAL_STACK_SIZE * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

static TickType_t xReceiveTimeOut         = pdMS_TO_TICKS( 4000 );
static TickType_t xSendTimeOut            = pdMS_TO_TICKS( 4000 );
static BaseType_t xTrueValue              = 1;
static uint32_t   guestCounter;

// the webpage top header with rm logo
static const char *webpage_top = {
   "HTTP/1.1 200 OK\r\n"
   "Content-Type: text/html\r\n\r\n" //"Content-Type: text/html; charset=utf-8\r\n"
   //"Keep-Alive: timeout=20\r\n" 
   //"Connection: keep-alive\r\n\r\n"

   "<html><head>"
      
   "<style type='text/css'>"
   // header text
   "#box1 {background-color: orange; background-image: linear-gradient(red, black);}"
   "</style>"
      
   FAVICON
   "<title>RNDIS Webserver Example</title>"
   "<style> div.main {"
   "font-family: Arial;"
   "padding: 0.01em 30px;"
   "box-shadow: 2px 2px 1px 1px #d2d2d2;"
   "background-color: #f1f1f1;}"
   "</style>"
   "</head>"
   "<body><div class='main'>"
   // Top header bar ////////////////////////////////////////////////////////
   "<p><div id='box1' style='height:55;'>"
   "<font size='20' font color='white'><b>RNDIS Webserver Example</b></font>"
   "</div></p>"
   //////////////////////////////////////////////////////////////////////////
   "<br />"
};

static const char *webpage_bottom_no_btn = {
   "<br>"    
   "<p style=\"text-align:center;\">&#169 Copyright 2021 by Nico Korn</p>" 
   "</div></body></html>"
};

// Global variables ***********************************************************

// Private function prototypes ************************************************
static void       webserver_listen        ( void *pvParameters );
static void       webserver_handle        ( void *pvParameters );
static void       webserver_homepage      ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static uint16_t   webserver_favicon       ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_205           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_204           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_204Refresh    ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_201           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_200           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_301           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_400           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       webserver_lastPacket    ( Socket_t xConnectedSocket );

// Functions ******************************************************************

//------------------------------------------------------------------------------
/// \brief     Web server initialization function.
///
/// \param     none
///
/// \return    none
void webserver_init( void )
{
   // initialise webserver task
   webserverListenTaskToNotify = osThreadNew( webserver_listen, NULL, &webserverListenTask_attributes );
}

//------------------------------------------------------------------------------
/// \brief     Web server deinitialization function.
///
/// \param     none
///
/// \return    none
void webserver_deinit( void )
{
}

// ----------------------------------------------------------------------------
/// \brief     Creates a task which listen on http port 80 for requests
///
/// \param     [in]  void *pvParameters
///
/// \return    none
static void webserver_listen( void *pvParameters )
{
   struct freertos_sockaddr   xClient, xBindAddress;
   Socket_t                   xListeningSocket, xConnectedSocket;
   socklen_t                  xSize          = sizeof( xClient );
   const TickType_t           timeout        = portMAX_DELAY;
   const BaseType_t           xBacklog       = 4;
   
   /* Attempt to open the socket. */
   xListeningSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );

   /* Check the socket was created. */
   configASSERT( xListeningSocket != FREERTOS_INVALID_SOCKET );

   /* Set a time out so accept() will just wait for a connection. */
   FreeRTOS_setsockopt( xListeningSocket, 0, FREERTOS_SO_RCVTIMEO, &timeout, sizeof( timeout ) );

   /* Set the listening sblink to 80 as general for http. */
   xBindAddress.sin_port = ( uint16_t ) 80;
   xBindAddress.sin_port = FreeRTOS_htons( xBindAddress.sin_port );

   /* Bind the socket to the sblink that the client RTOS task will send to. */
   FreeRTOS_bind( xListeningSocket, &xBindAddress, sizeof( xBindAddress ) );

   /* Set the socket into a listening state so it can accept connections.
   The maximum number of simultaneous connections is limited to 20. */
   FreeRTOS_listen( xListeningSocket, xBacklog );

   for( ;; )
   {
      // Wait for incoming connections.
      xConnectedSocket = FreeRTOS_accept( xListeningSocket, &xClient, &xSize );
      configASSERT( xConnectedSocket != FREERTOS_INVALID_SOCKET );

      // Spawn a RTOS task to handle the connection.
      webserverHandleTaskToNotify = osThreadNew( webserver_handle, (void*)xConnectedSocket, &webserverHandleTask_attributes );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Creates a task when if tcp frame arrives from a session and
///            handles its content. 
///            Handling GET or POST requests and parsing the uri's.
///
/// \param     [in]  void *pvParameters
///
/// \return    none
static void webserver_handle( void *pvParameters )
{
   Socket_t          xConnectedSocket;
   uint8_t           uri[URIBUFFER];
   uint8_t           *sp1, *sp2; 
   TickType_t        xTimeOnShutdown;
   uint8_t           *pucRxBuffer;
   uint8_t           *pageBuffer;
   BaseType_t        lengthOfbytes;
   static uint16_t   etimeout;  
   static uint16_t   enomem;  
   static uint16_t   enotconn;
   static uint16_t   eintr;   
   static uint16_t   einval; 
   static uint16_t   eelse;
   static uint16_t   uriTooLongError;
   static uint8_t    serverInstanceMallocError;
   FlagStatus        jumpToAppFlag = RESET;
   
   // get the socket
   xConnectedSocket = ( Socket_t ) pvParameters;

	// allocate heap for the page
   pageBuffer = ( uint8_t * ) pvPortMalloc( TXBUFFERSIZE );
   
   if( pageBuffer == NULL )
   {
      memset(uri,0x00,URIBUFFER);
      webserver_400(uri, TXBUFFERSIZE, xConnectedSocket);
      memset(uri,0x00,URIBUFFER);
      FreeRTOS_shutdown( xConnectedSocket, FREERTOS_SHUT_RDWR );    
      xTimeOnShutdown = xTaskGetTickCount();
      do
      {
         if( FreeRTOS_recv( xConnectedSocket, uri, URIBUFFER, 0 ) < 0 )
         {
            vTaskDelay( pdMS_TO_TICKS( 250 ) );
            break; 
         }
      } while( ( xTaskGetTickCount() - xTimeOnShutdown ) < pdMS_TO_TICKS( 5000 ) );
      serverInstanceMallocError++;
      FreeRTOS_closesocket( xConnectedSocket );
      vTaskDelete( NULL );
      return;
   }
   
   // allocate heap for frame reception
	pucRxBuffer = ( uint8_t * ) pvPortMalloc( ipconfigTCP_MSS );
   
   if( pucRxBuffer == NULL )
   {
      vPortFree( pageBuffer );
      memset(uri,0x00,URIBUFFER);
      webserver_400(uri, TXBUFFERSIZE, xConnectedSocket);
      memset(uri,0x00,URIBUFFER);
      FreeRTOS_shutdown( xConnectedSocket, FREERTOS_SHUT_RDWR );    
      xTimeOnShutdown = xTaskGetTickCount();
      do
      {
         if( FreeRTOS_recv( xConnectedSocket, uri, URIBUFFER, 0 ) < 0 )
         {
            vTaskDelay( pdMS_TO_TICKS( 250 ) );
            break; 
         }
      } while( ( xTaskGetTickCount() - xTimeOnShutdown ) < pdMS_TO_TICKS( 5000 ) );
      serverInstanceMallocError++;
      FreeRTOS_closesocket( xConnectedSocket );
      vTaskDelete( NULL );
      return;
   }

   //FreeRTOS_setsockopt( xConnectedSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
	//FreeRTOS_setsockopt( xConnectedSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

   for( ;; )
   {
      // Receive data on the socket.
      lengthOfbytes = FreeRTOS_recv( xConnectedSocket, pucRxBuffer, ipconfigTCP_MSS, 0 );
         
      // check lengthOfbytes ------------- lengthOfbytes > 0                           --> data received
      //                                   lengthOfbytes = 0                           --> timeout
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_ENOMEM     --> not enough memory on socket
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_ENOTCONN   --> socket was or got closed
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_EINTR      --> if the socket received a signal, causing the read operation to be aborted
      //                                   lengthOfbytes = pdFREERTOS_ERRNO_EINVAL     --> socket is not valid
      if( lengthOfbytes > 0 )
      {         
         /* check for a GET request */
         if (!strncmp((char const*)pucRxBuffer, "GET ", 4))
         {
            // extract URI
            sp1 = pucRxBuffer + 4;
            sp2 = memchr(sp1, ' ', URIBUFFER);
            uint16_t len = sp2 - sp1;
            if(len>URIBUFFER)
            {
               // uri too long
               uriTooLongError++;

               // listen to the socket again
               continue; 
            }
            memset(uri, 0x00, URIBUFFER);
            memcpy(uri, sp1, len);
            uri[len] = '\0';
            
            if(memcmp((char const*)uri, "/home", 5u) == 0)
            {
               // mainpage
               webserver_homepage( pageBuffer, TXBUFFERSIZE, xConnectedSocket );
               
               // listen to the socket again
               continue;
            }
            else
            {
               // send 400
               webserver_homepage( pageBuffer, TXBUFFERSIZE, xConnectedSocket );
               
               // listen to the socket again
               continue;
            }
         }
         
         /* check for a POST request */
         if (!strncmp((char const*)pucRxBuffer, "POST ", 5u)) {
            
            // extract URI
            sp1 = pucRxBuffer + 5;
            sp2 = memchr(sp1, ' ', URIBUFFER);
            uint16_t len = sp2 - sp1;
            if(len>URIBUFFER)
            {
               // uri too long
               uriTooLongError++;

               // listen to the socket again
               continue; 
            }
            memset(uri, 0x00, URIBUFFER);
            memcpy(uri, sp1, len);
            uri[len] = '\0';
            
            if (strncmp((char const*)uri, "/led_toggle", 10u) == 0)
            {
               // toggle led
               led_toggle();
               
               webserver_204(pageBuffer, TXBUFFERSIZE, xConnectedSocket);

               // listen to the socket again
               continue; 
            }
            else if (strncmp((char const*)uri, "/led_set_value/", 15u) == 0)
            {
               uint8_t value;
               
               // set led into dim state
               led_setDim();
               
               // toggle led
               value = strtol( (char const*)&uri[15u], NULL, 10u );
               
               // check if value is valid
               if( value <= 40 )
               {
                  // set led pwm
                  led_setDuty(value);
               }
               
               // send ok rest api
               webserver_204(pageBuffer, TXBUFFERSIZE, xConnectedSocket);

               // listen to the socket again
               continue; 
            }
            else if (strncmp((char const*)uri, "/led_pulse", 9u) == 0)
            {
               // set led into pulse state
               led_setPulse();
               
               // send ok rest api
               webserver_204(pageBuffer, TXBUFFERSIZE, xConnectedSocket);

               // listen to the socket again
               continue; 
            }
            else
            {
               // send bad request if the uri is faulty
               webserver_400(pageBuffer, TXBUFFERSIZE, xConnectedSocket);
               
               // listen to the socket again
               continue; 
            }
         }
      }
      else if( lengthOfbytes == 0 )
      {
         // No data was received, but FreeRTOS_recv() did not return an error. Timeout?
         etimeout++;                                                       
         break;    
      }
      else if( lengthOfbytes == pdFREERTOS_ERRNO_ENOMEM )                                                                                        
      {                                                                                                                    
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                   
         enomem++;                                                      
         break;
      } 
      else if( lengthOfbytes == pdFREERTOS_ERRNO_ENOTCONN )                                                                   
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         enotconn++;                                                        
         break;
      } 
      else if( lengthOfbytes == pdFREERTOS_ERRNO_EINTR )                                                                      
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         eintr++;                                                        
         break;
      } 
      else if( lengthOfbytes == pdFREERTOS_ERRNO_EINVAL )                                                                      
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         einval++;                                                          
         break;
      } 
      else
      {                                                                                                                       
         // Error (maybe the connected socket already shut down the socket?). Attempt graceful shutdown.                      
         eelse++;                                                        
         break;
      } 
   }
   
   // The RTOS task will get here if an error is received on a read.  Ensure the
   // socket has shut down (indicated by FreeRTOS_recv() returning a FREERTOS_EINVAL
   // error before closing the socket).
   // Wait for the shutdown to take effect, indicated by FreeRTOS_recv()
   // returning an error.
   xTimeOnShutdown = xTaskGetTickCount();
   do
   {
      if( FreeRTOS_recv( xConnectedSocket, pucRxBuffer, ipconfigTCP_MSS, 0 ) < 0 )
      {
         vTaskDelay( pdMS_TO_TICKS( 250 ) );
         break;
      }
   } while( ( xTaskGetTickCount() - xTimeOnShutdown ) < pdMS_TO_TICKS( 5000 ) );
   
   /* Finished with the socket, buffer, the task. */
   vPortFree( pucRxBuffer );
   vPortFree( pageBuffer );
   FreeRTOS_closesocket( xConnectedSocket );
   
   vTaskDelete( NULL );
}

// ----------------------------------------------------------------------------
/// \brief     Main page of the device. With an overview of important data.
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    none
static void webserver_homepage( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   uint16_t 		   stringLength;
   uint32_t 		   totalSeconds;
   uint32_t 		   days;
   uint32_t 		   hours;
   uint32_t 		   minutes;
   uint32_t 		   seconds;
   uint8_t  		   *ipAddress8b;
   uint32_t 		   ipAddress;
   uint32_t 		   netMask;
   uint32_t 		   dnsAddress;
   uint32_t 		   gatewayAddress;
   size_t 		      freeheap;
   const uint8_t* 	stackMacAddress;
   osThreadId_t      tasks[7] = {0};
   uint32_t          tasksNr = 7;
   uint8_t           dutyCycle;
   uint8_t           taskCount;
   TaskStatus_t      *task;
   float             temperature;
   float             voltage;
   
   static const char *webpage_panelcontrollerMonitor = {
      
      "<style type='text/css'>"
      // header text
      "#box1 {background-color: orange; background-image: linear-gradient(red, black);}"
      // slider
      ".slidecontainer {width: 100%;}"
      ".slider {-webkit-appearance: none;width: 250px;height: 15px;border-radius: 5px;background: #d3d3d3;outline: none;opacity: 0.7;-webkit-transition: .2s;transition: opacity .2s;}"
      ".slider:hover {opacity: 1;}"
      ".slider::-moz-range-thumb {width: 25px;height: 25px;border-radius: 50%;background: #04AA6D;cursor: pointer;}"
      "</style>"
      
      "<p><h3>Homepage</h3></p>"
      "<p>___</p>"
      "<p>IP: %d.%d.%d.%d</p>"
      "<p>MAC: %02x:%02x:%02x:%02x:%02x:%02x</p>"
      "<p>Uptime: %d days, %d hours, %d minutes, %d seconds</p>"
      "<p>___</p>"
      "<p>FreeRTOS Version: %s</p>"
      "<p>Free Heap: %d bytes</p>"
      "<p>Running Tasks</p>"
      "<p>- Task 1: %s, Priority: %d</p>"
      "<p>- Task 2: %s, Priority: %d</p>"
      "<p>- Task 3: %s, Priority: %d</p>"
      "<p>- Task 4: %s, Priority: %d</p>"
      "<p>- Task 5: %s, Priority: %d</p>"
      "<p>- Task 6: %s, Priority: %d</p>"
      "<p>- Task 7: %s, Priority: %d</p>"
      "<p>___</p>"
         
      "<p><div class='slidecontainer'>"
         "Led Dimmer<input type='range' min='1' max='40' value=%d class='slider' id='myRange'>"
      "</div></p>" 
         
      "<script>"
      "var xhr=new XMLHttpRequest();"
      "var slider=document.getElementById('myRange');"
      "var block=0;"
         
      "function unblock(){block=0;}"
      
      "slider.oninput=function(){"
         "if(block==0){"
            "block=1;"
            "var urlpost='/led_set_value/'+this.value;"
            "xhr.open('POST', urlpost, true);"
            "xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');"
            "xhr.send('zero');"
            "setTimeout(unblock, 250);"
         "}"
      "}"
      "</script>"
          
      "<p><form action='led_pulse' method='post'><button  style='width:200px'>Led Pulse</button></form></p>"
      "<p>Temperature: %.1f C"
      "<p>Voltage: %.2f V</p>"
      "<p>___</p>"
      "<p>Guest counter: %d</p>" 
      "<br />"
   };
   
   // set the placeholder in every webpage fragment and put all fragments together
   // top header
   stringLength = snprintf(0, 0, webpage_top);
   snprintf((char*)pageBuffer, stringLength+1, webpage_top);
   
   // send fragment of the webpage//////////////////////////////////////////////
   if( stringLength < pageBufferSize )
   {
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
   /////////////////////////////////////////////////////////////////////////////
   
   // panelcontroller monitor
   FreeRTOS_GetAddressConfiguration( &ipAddress, &netMask, &gatewayAddress, &dnsAddress );
   ipAddress8b       = (uint8_t*)(&ipAddress);
   temperature       = monitor_getTemperature();
   voltage           = monitor_getVoltage();
   stackMacAddress   = FreeRTOS_GetMACAddress();
   totalSeconds      = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
   days              = (totalSeconds / 86400);       
   hours             = (totalSeconds / 3600) % 24;   
   minutes           = (totalSeconds / 60) % 60;     
   seconds           = totalSeconds % 60;  
   freeheap          = xPortGetFreeHeapSize();
   taskCount         = uxTaskGetNumberOfTasks();
   task              = pvPortMalloc(taskCount * sizeof(TaskStatus_t));
   if (task != NULL)
   {
      taskCount = uxTaskGetSystemState(task, taskCount, NULL);
   }
   else
   {
      return;
   }
   dutyCycle = led_getDuty();
   guestCounter++;
   stringLength = snprintf(0, 0, webpage_panelcontrollerMonitor, 
                           ipAddress8b[0], ipAddress8b[1], ipAddress8b[2], ipAddress8b[3], 
                           stackMacAddress[0], stackMacAddress[1], stackMacAddress[2], stackMacAddress[3], stackMacAddress[4], stackMacAddress[5], 
                           days, hours, minutes, seconds, 
                           tskKERNEL_VERSION_NUMBER,
                           freeheap, 
                           task[0].pcTaskName, task[0].uxCurrentPriority, 
                           task[1].pcTaskName, task[1].uxCurrentPriority,  
                           task[2].pcTaskName, task[2].uxCurrentPriority,  
                           task[3].pcTaskName, task[3].uxCurrentPriority,  
                           task[4].pcTaskName, task[4].uxCurrentPriority,  
                           task[5].pcTaskName, task[5].uxCurrentPriority, 
                           task[6].pcTaskName, task[6].uxCurrentPriority, 
                           dutyCycle, 
                           temperature,
                           voltage,
                           guestCounter );
   snprintf((char*)pageBuffer, stringLength+1, webpage_panelcontrollerMonitor, 
                           ipAddress8b[0], ipAddress8b[1], ipAddress8b[2], ipAddress8b[3], 
                           stackMacAddress[0], stackMacAddress[1], stackMacAddress[2], stackMacAddress[3], stackMacAddress[4], stackMacAddress[5], 
                           days, hours, minutes, seconds, 
                           tskKERNEL_VERSION_NUMBER,
                           freeheap, 
                           task[0].pcTaskName, task[0].uxCurrentPriority, 
                           task[1].pcTaskName, task[1].uxCurrentPriority,  
                           task[2].pcTaskName, task[2].uxCurrentPriority,  
                           task[3].pcTaskName, task[3].uxCurrentPriority,  
                           task[4].pcTaskName, task[4].uxCurrentPriority,  
                           task[5].pcTaskName, task[5].uxCurrentPriority, 
                           task[6].pcTaskName, task[6].uxCurrentPriority,         
                           dutyCycle, 
                           temperature,
                           voltage,
                           guestCounter );
   
   // send fragment of the webpage//////////////////////////////////////////////
   if( stringLength < pageBufferSize )
   {
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
   /////////////////////////////////////////////////////////////////////////////
   
   // bottom
   stringLength = snprintf(0, 0, webpage_bottom_no_btn);
   snprintf((char*)pageBuffer, stringLength+1, webpage_bottom_no_btn);
   
   // send fragment of the webpage//////////////////////////////////////////////
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
   vPortFree(task);
   /////////////////////////////////////////////////////////////////////////////
}

// ----------------------------------------------------------------------------
/// \brief     Returns the favicon on http request.
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    stringLength
static uint16_t webserver_favicon( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   // general page variables
   uint16_t stringLength;
   static const char *httpFavicon = {
   "HTTP/1.1 200 OK\r\n"
   "Content-Type: text/html\r\n" //"Content-Type: text/html; charset=utf-8\r\n"
   "Connection: close\r\n\r\n"
   "<html><head>"
   "<link rel=\"shortcut icon\" href=\"data:image/gif;base64,R0lGODlhIAAgAHAAACwAAAAAIAAgAIf////s2+Pqt8j/wNT/v836wsv/xM7/wM7/w9L6ws38uMP4ZHTQCyfgAB7cABnXBhzeBh7bABjiAR/iAB3UAC3wdYfZAwvdAybkAyHqBivjABrgAyblASbiASHjAiLkACXwZ4XkABHsBDTgBCjVASnfASbgBS3eACnhACjiASnXBR7tfpHdAxHpAC3sAifiACXtACXiACbfACXjASfjABnmY3XbBgrYAB7qAB3tAB3tABrgAB3iAB7hAB7ZCCX31+z7vbLjycztw8T0v8fxw8Puysr3vsf1wMj/s6/6v93+m57oo7Xxnaz/nrX3m6rworD9mbH5nLH6nbL1pKPhaXLbBQPfABvhABDrAx3pABDeABDjABbfABXgARbdASXgfY/iABD9AjfcACLTByjoByfgAiffAyfkABvya4ngABDvAzPUAyLfASjjAiTlACndASfeACXkARzleIvZBRHqACvhACLoACjeASDnASjdBCHpaH7gDhfkAyvkBSLqACfeBSDgBybnBSrkBCbjACn67//3x7vz2N354Nz/0+D62t310tj72dr62Nn9zcH4rc7zgobqjKTskJ/6gaDvjqLsiKDwip/viZ7lhYnuanjjDhDjACDhABfmABjdARrnAh/hABniABruAS/ddILTBAzqACjiBiLsCC3hASPkAyPlBCTYAh72b4/iABXwBDXUAyPeACvlACvdACvdACbeACfgAinhBR7vdXLVABnnABvSARflBSDdBB/mAB/vaHnmEz70FULfFzvsFD3lFD7pCzrnEDrVFDMAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAI/wABCBxIsKDBgwgTKlzIsKHDhxAjPvwhABEBAgYIWDTAsWPGhwEcXeR4caQAjCcVPKyRS4uWllrEuJQpE4aWNQ8rxBSjRowYGD6BAv0p5iEILT19xopFtKnPh5a0wOgJYyhPnz3VaFlZZU2WqkFpii3q8IeCIEWKFCoiwMDJIigJAHmYRMkTJk2gPIHCly9evlMeUqniMksWwi4TJybbUJSWMGLWXFUqhinTrQ6PqlkTayhTykEfOhaK9SfVqU8zZ2I6VM1Qq0AhFklEKFGiQwFu07Ztm9FDRxUsWZIECZJw4Y+OW7L1sBfiLC5tztQCHanoUVl9EgUrNJdRpKU/Ky5dyvShLTFZcml/7TPLT8wNexX7JezXfDXFiOknZp9YMYkABijggAQWaOCBAAQEADs=\" />"   
   "</body><body>"
   "</head></html>"
   };
   
   // generate message
   stringLength = snprintf(0, 0, httpFavicon);
   snprintf((char*)pageBuffer, stringLength+1, httpFavicon);
   
   // send fragment of the webpage
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
   return stringLength;
}

// ----------------------------------------------------------------------------
/// \brief     Returns the REST API 200 status code ok
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    none
static void webserver_200( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   // general page variables
   uint16_t stringLength;
   static const char *httpCode200 = {
      "HTTP/1.1 200 OK\r\n"
      "Content-Length: 0\r\n\r\n"
   };
   
   // generate message
   stringLength = snprintf(0, 0, httpCode200);
   snprintf((char*)pageBuffer, stringLength+1, httpCode200);
   
   // send fragment of the webpage
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Returns the REST API 204 status code no content
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    none
static void webserver_204( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   // general page variables
   uint16_t stringLength;
   static const char *httpCode204 = {
      "HTTP/1.1 204 No Content\r\n"
      "Connection: close\r\n\r\n"
   };
   
   // generate message
   stringLength = snprintf(0, 0, httpCode204);
   snprintf((char*)pageBuffer, stringLength+1, httpCode204);
   
   // send fragment of the webpage
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Returns the REST API 205 status code reset ui
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    none
static void webserver_205( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   // general page variables
   uint16_t stringLength;
   static const char *httpCode205 = {
      "HTTP/1.1 205 Reset Content\r\n"
      "Content-Length: 0\r\n\r\n"
   };
   
   // generate message
   stringLength = snprintf(0, 0, httpCode205);
   snprintf((char*)pageBuffer, stringLength+1, httpCode205);
   
   // send fragment of the webpage
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Returns the REST API 201 status code created
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    none
static void webserver_201( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   // general page variables
   uint16_t stringLength;
   static const char *httpCode201 = {
      "HTTP/1.1 201 Created\r\n"
      //"Refresh: 0\r\n"
      "Refresh: 0, url= \r\n"
      "Content-Length: 0\r\n\r\n"
   };
   
   // generate message
   stringLength = snprintf(0, 0, httpCode201);
   snprintf((char*)pageBuffer, stringLength+1, httpCode201);
   
   // send fragment of the webpage
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Returns the REST API 400 status code bad request
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    none
static void webserver_400( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   // general page variables
   uint16_t stringLength;
   static const char *httpCode400 = {
      "HTTP/1.1 400 Bad Request\r\n"
      "Content-Length: 0\r\n\r\n"
   };
   
   // generate message
   stringLength = snprintf(0, 0, httpCode400);
   snprintf((char*)pageBuffer, stringLength+1, httpCode400);
   
   // send fragment of the webpage
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Returns the REST API 301 status code moved permanently
///
/// \param     [in]  uint8_t* pageBuffer
/// \param     [in]  uint16_t pageBufferSize
/// \param     [in]  Socket_t xConnectedSocket
///
/// \return    none
static void webserver_301( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
{
   // general page variables
   uint16_t stringLength;
   static const char *httpCode301 = {
      "HTTP/1.1 301 Moved Permanently\r\n"
      "Location: /home\r\n" 
      "Content-Length: 0\r\n\r\n"
   };

   // generate message
   stringLength = snprintf(0, 0, httpCode301 );
   snprintf((char*)pageBuffer, stringLength+1, httpCode301 );
   
   // send fragment of the webpage
   if( stringLength < pageBufferSize )
   {
      webserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Last packet sets fin option on socket
///
/// \param     none
///
/// \return    none
static void webserver_lastPacket( Socket_t xConnectedSocket )
{
   xTrueValue = 1;
   FreeRTOS_setsockopt( xConnectedSocket, 0, FREERTOS_SO_CLOSE_AFTER_SEND, ( void * ) &xTrueValue, sizeof( xTrueValue ) );
}
/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/