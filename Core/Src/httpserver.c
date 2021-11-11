// ****************************************************************************
/// \file      webserver.c
///
/// \brief     web server c file
///
/// \details   Handles webserver requests from clients.
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
#include "httpserver.h"
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

// the webpage top header
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
   "<title>RNDIS HTTP Server Example</title>"
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
   "<font size='20' font color='white'><b>RNDIS HTTP Server Example</b></font>"
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
static void       httpserver_listen        ( void *pvParameters );
static void       httpserver_handle        ( void *pvParameters );
static void       httpserver_homepage      ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static uint16_t   httpserver_favicon       ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_205           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_204           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_204Refresh    ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_201           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_200           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_301           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_400           ( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket );
static void       httpserver_lastPacket    ( Socket_t xConnectedSocket );

// Functions ******************************************************************

//------------------------------------------------------------------------------
/// \brief     Web server initialization function.
///
/// \param     none
///
/// \return    none
void httpserver_init( void )
{
   // initialise webserver task
   webserverListenTaskToNotify = osThreadNew( httpserver_listen, NULL, &webserverListenTask_attributes );
}

//------------------------------------------------------------------------------
/// \brief     Web server deinitialization function.
///
/// \param     none
///
/// \return    none
void httpserver_deinit( void )
{
   osThreadTerminate(&webserverListenTaskToNotify);
}

// ----------------------------------------------------------------------------
/// \brief     Creates a task which listen on http port 80 for requests
///
/// \param     [in]  void *pvParameters
///
/// \return    none
static void httpserver_listen( void *pvParameters )
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
      webserverHandleTaskToNotify = osThreadNew( httpserver_handle, (void*)xConnectedSocket, &webserverHandleTask_attributes );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Handles REST API GET and POST request to the http server.
///
/// \param     [in]  void *pvParameters
///
/// \return    none
static void httpserver_handle( void *pvParameters )
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
      httpserver_400(uri, TXBUFFERSIZE, xConnectedSocket);
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
      httpserver_400(uri, TXBUFFERSIZE, xConnectedSocket);
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
         // check for a GET request
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
               httpserver_homepage( pageBuffer, TXBUFFERSIZE, xConnectedSocket );
               
               // listen to the socket again
               continue;
            }
            else
            {
               // send 400
               httpserver_homepage( pageBuffer, TXBUFFERSIZE, xConnectedSocket );
               
               // listen to the socket again
               continue;
            }
         }
         
         // check for a POST request
         if (!strncmp((char const*)pucRxBuffer, "POST ", 5u))
         {
            
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
               
               httpserver_204(pageBuffer, TXBUFFERSIZE, xConnectedSocket);

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
               httpserver_204(pageBuffer, TXBUFFERSIZE, xConnectedSocket);

               // listen to the socket again
               continue; 
            }
            else if (strncmp((char const*)uri, "/led_pulse", 9u) == 0)
            {
               // set led into pulse state
               led_setPulse();
               
               // send ok rest api
               httpserver_204(pageBuffer, TXBUFFERSIZE, xConnectedSocket);

               // listen to the socket again
               continue; 
            }
            else
            {
               // send bad request if the uri is faulty
               httpserver_400(pageBuffer, TXBUFFERSIZE, xConnectedSocket);
               
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
static void httpserver_homepage( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
         "Led Dimmer<input type='range' min='2' max='40' value=%d class='slider' id='myRange'>"
      "</div></p>" 
         
      "<script>"
      "var xhr=new XMLHttpRequest();"
      "var slider=document.getElementById('myRange');"
      "var block=0;"
         
      "function unblock(){block=0;}"
      
      // block next post request triggert by the slider for 250 ms to avoid "spamming"
      "slider.oninput=function(){"
         "if(block==0){"
            "block=1;"
            "var urlpost='/led_set_value/'+this.value;"
            "xhr.open('POST', urlpost, true);"
            "xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded; charset=UTF-8');"
            "xhr.send('zero');"
            "setTimeout(unblock, 150);"
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
      httpserver_lastPacket( xConnectedSocket );
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
static uint16_t httpserver_favicon( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
      httpserver_lastPacket( xConnectedSocket );
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
static void httpserver_200( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
      httpserver_lastPacket( xConnectedSocket );
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
static void httpserver_204( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
      httpserver_lastPacket( xConnectedSocket );
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
static void httpserver_205( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
      httpserver_lastPacket( xConnectedSocket );
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
static void httpserver_201( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
      httpserver_lastPacket( xConnectedSocket );
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
static void httpserver_400( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
      httpserver_lastPacket( xConnectedSocket );
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
static void httpserver_301( uint8_t* pageBuffer, uint16_t pageBufferSize, Socket_t xConnectedSocket )
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
      httpserver_lastPacket( xConnectedSocket );
      FreeRTOS_send( xConnectedSocket, pageBuffer, stringLength, 0 );
   }
}

// ----------------------------------------------------------------------------
/// \brief     Last packet sets fin option on socket.
///
/// \param     none
///
/// \return    none
static void httpserver_lastPacket( Socket_t xConnectedSocket )
{
   xTrueValue = 1;
   FreeRTOS_setsockopt( xConnectedSocket, 0, FREERTOS_SO_CLOSE_AFTER_SEND, ( void * ) &xTrueValue, sizeof( xTrueValue ) );
}
/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/