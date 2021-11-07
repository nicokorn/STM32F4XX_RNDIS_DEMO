// ****************************************************************************
/// \file      queuex.c
///
/// \brief     queue Module
///
/// \details   This is the queuex c source file. It contents the queue
///            functionality. The queue is a ringbuffer with static allocated
///            memory for the sake of performance. The parameters are in the 
///            header file. There the parameters for the ringbuffer length and
///            buffer length can be set.
///
/// \author    Nico Korn
///
/// \version   0.3.0.0
///
/// \date      07112021
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


// Include ********************************************************************
//#include "usb.h"
#include "queuex.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Private define *************************************************************

// Private types     **********************************************************

// Private variables **********************************************************

// Private functions **********************************************************

// ----------------------------------------------------------------------------
/// \brief     Queue init.
///
/// \param     [in/out] queue_handle_t *queueHandle
///
/// \return    none
void queue_init( queue_handle_t *queueHandle )
{   
   // disable irq to avoid racing conditions
   __disable_irq(); 
   
   // init statistics to 0
   queueHandle->dataPacketsIN          = 0;
   queueHandle->bytesIN                = 0;
   queueHandle->dataPacketsOUT         = 0;
   queueHandle->bytesOUT               = 0;
   queueHandle->queueFull              = 0;
   queueHandle->queueLengthPeak        = 0;
   queueHandle->queueLength            = 0;
   queueHandle->headIndex              = QUEUELENGTH;
   queueHandle->tailIndex              = QUEUELENGTH;
   
   // cleanup the queue
   for( uint8_t i = 0; i < QUEUELENGTH; i++ )
   {
      memset( queueHandle->queue[i].data, 0x00, QUEUEBUFFERLENGTH );
      queueHandle->queue[i].dataLength       = 0;
      queueHandle->queue[i].messageStatus    = EMPTY_TX;
      queueHandle->queue[i].dataStart        = NULL;
   }
   
   // queue status - the tail is used to transmitt messages. As long as the
   // peripheral is sending the tail thus the queue status remains TAIL_BLOCKED 
   // because of doing zero opy. After a transmission has been completed the 
   // queue status will be set back to TAIL_UNBLOCKED.
   queueHandle->queueStatus            = TAIL_UNBLOCKED;
   
   // disable irq
   __enable_irq(); 
}

// ----------------------------------------------------------------------------
/// \brief     The queue manager checks for available data to send, and calls
///            the linked peripheral output interface. NOTE! You have to provide
///            and link an output interface function before calling this
///            function.
///
/// \param     [in/out] queue_handle_t *queueHandle
///
/// \return    none
inline void queue_manager( queue_handle_t *queueHandle )
{      
   // If the queue status is set to a blocked tail return.
   if( queueHandle->queueStatus != TAIL_UNBLOCKED )
   {
      return;
   }
   
   // Check if tail and header index are ok and if the message object in the 
   // queue is ready for transmission.
   if( queueHandle->tailIndex < queueHandle->headIndex 
      && queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].messageStatus == READY_FOR_TX )
   {
      // To avoid racing conditions, immediately block the tail and set the
      // message status to processing.
      queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].messageStatus = PROCESSING_TX;
      queueHandle->queueStatus = TAIL_BLOCKED;
      
      // Send the frame with the linked output function provided by the
      // communication peripheral.
      if( queueHandle->output( queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].dataStart, queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].dataLength ) != 1 )
      {
         // Peripheral is busy, set back states.
         queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].messageStatus = READY_FOR_TX;
         queueHandle->queueStatus = TAIL_UNBLOCKED;
      }
   }
}

// ----------------------------------------------------------------------------
/// \brief     The queue manager checks for available data to send, and calls
///            the linled output interface.
///
/// \param     [in/out] queue_handle_t *queueHandle
///
/// \return    none
inline void queue_dequeue( queue_handle_t *queueHandle )
{   
   // Transmission complete unblock the tail.
   queueHandle->queueStatus = TAIL_UNBLOCKED;
  
   if( queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].messageStatus != PROCESSING_TX )
   {
      // Spurious error check for debugging
      queueHandle->spuriousError++;
      return;
   }
   
   // Check if tail and head index are ok.
   if( queueHandle->tailIndex < queueHandle->headIndex )
   {
      // Update queue statistics.
      queueHandle->dataPacketsOUT++;
      queueHandle->bytesOUT += queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].dataLength; // note: this are the frame bytes without preamble and crc value
      
      // Set message status.
      queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].data[0] = 0x00;
      queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].messageStatus = EMPTY_TX;
      
      // Set tail number.
      queueHandle->tailIndex++;
      
      // set queue length
      queueHandle->queueLength = queueHandle->headIndex - queueHandle->tailIndex;
   }
   else
   {
      queueHandle->tailError++;
   }
}

// ----------------------------------------------------------------------------
/// \brief     Enqueue a new message into the ringbuffer. If the ringbuffer is
///            full, the slot will be used again until the head can move
///            forward.
///
/// \param     [in/out] queue_handle_t *queueHandle
///
/// \return    uint8_t* data pointer
inline uint8_t* queue_enqueue( uint8_t* dataStart, uint16_t dataLength, queue_handle_t *queueHandle )
{
   // Integer type wrap arround check and set for head and tail index.
   if( queueHandle->headIndex < QUEUELENGTH )
   {
      queueHandle->headIndex += QUEUELENGTH;
      queueHandle->tailIndex += QUEUELENGTH;
   }
   
   // Ringbuffer not full?
   if( (queueHandle->headIndex - queueHandle->tailIndex) < QUEUELENGTH-1 )
   {
      // Get actual index in the ringbuffer.
      uint8_t arrayIndex = queueHandle->headIndex%QUEUELENGTH;
      
      // Set data length in the message object.
      queueHandle->queue[arrayIndex].dataLength = dataLength;
      
      // Set data start pointer in the databuffer of the message object.
      queueHandle->queue[arrayIndex].dataStart = dataStart;
      
      // Set message status in the message object.
      queueHandle->queue[arrayIndex].messageStatus = READY_FOR_TX;
      
      // Update queue statistics.
      queueHandle->frameCounter++;
      queueHandle->dataPacketsIN++;
      queueHandle->bytesIN += dataLength;
      if( queueHandle->queueLength > queueHandle->queueLengthPeak )
      {
         queueHandle->queueLengthPeak = queueHandle->queueLength;
      }
      
      // Increment absolute head index
      queueHandle->headIndex++;
      
      // set queue length
      queueHandle->queueLength = queueHandle->headIndex - queueHandle->tailIndex;
      
      // Set receiving state on the queue object.
      queueHandle->queue[queueHandle->headIndex%QUEUELENGTH].messageStatus = RECEIVING_RX;

      // Return new pointer.
      return queueHandle->queue[queueHandle->headIndex%QUEUELENGTH].data;
   }

   // Queue is full, return old pointer.
   queueHandle->queueFull++;
   return queueHandle->queue[queueHandle->headIndex%QUEUELENGTH].data;
}

// ----------------------------------------------------------------------------
/// \brief     Returns pointer to the head buffer of the queue.
///
/// \param     [in/out] queue_handle_t *queueHandle
///
/// \return    uint8_t* data pointer
uint8_t* queue_getHeadBuffer( queue_handle_t *queueHandle )
{
   return queueHandle->queue[queueHandle->headIndex%QUEUELENGTH].data;
}

// ----------------------------------------------------------------------------
/// \brief     Returns pointer to the tail buffer of the queue.
///
/// \param     [in/out] queue_handle_t *queueHandle
///
/// \return    uint8_t* data pointer
uint8_t* queue_getTailBuffer( queue_handle_t *queueHandle )
{
   return queueHandle->queue[queueHandle->tailIndex%QUEUELENGTH].data;
}

// ----------------------------------------------------------------------------
/// \brief     Check if queue is full.
///
/// \param     [in/out] queue_handle_t *queueHandle
///
/// \return    0 = full, 1 = not full
uint8_t queue_isFull( queue_handle_t *queueHandle )
{
      // Ringbuffer not full?
   if( (queueHandle->headIndex - queueHandle->tailIndex) < QUEUELENGTH-1 )
   {
      return 1;
   }
   return 0;
}

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/