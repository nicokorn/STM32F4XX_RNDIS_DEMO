// ****************************************************************************
/// \file      crc32.c
///
/// \brief     crc module
///
/// \details   Needded if CRC32 shall be calculated if length%4!=0 and the
///            hardware crc32 cannot handle such cases.
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
#include "crc32.h"

// Private defines ************************************************************

// Private types     **********************************************************

// Private variables **********************************************************

// Global variables ***********************************************************

// Private function prototypes ************************************************
static uint32_t crc32_handle8( CRC_HandleTypeDef *hcrc, uint8_t pBuffer[], uint32_t BufferLength );

// Functions ******************************************************************

//------------------------------------------------------------------------------
/// \brief     CRC32 initialization function.
///
/// \param     none
///
/// \return    none
void crc32_init( void )
{
}

//------------------------------------------------------------------------------
/// \brief     CRC32 deinitialization function.
///
/// \param     none
///
/// \return    none
void crc32_deinit( void )
{
}

/**
  * @brief  Compute the 32-bit CRC value of a 32-bit data buffer
  *         starting with hcrc->Instance->INIT as initialization value.
  * @param  hcrc CRC handle
  * @param  pBuffer pointer to the input data buffer.
  * @param  BufferLength input data buffer length (number of uint32_t words).
  * @retval uint32_t CRC (returned value LSBs for CRC shorter than 32 bits)
  */
uint32_t crc32_calculate(CRC_HandleTypeDef *hcrc, uint32_t pBuffer[], uint32_t BufferLength)
{
  static uint32_t index;      /* CRC input data buffer index */
  volatile uint32_t temp = 0U;  /* CRC output (read from hcrc->Instance->DR register) */
  volatile  uint8_t *pBuffer8B;

  /* Change CRC peripheral state */
  hcrc->State = HAL_CRC_STATE_BUSY;

  /* Reset CRC Calculation Unit (hcrc->Instance->INIT is
  *  written in hcrc->Instance->DR) */
  __HAL_CRC_DR_RESET(hcrc);
  
  /* Enter 32-bit input data to the CRC calculator */
  /*
  for (index = 0U; index < BufferLength; index++)
  {
    *(__IO uint8_t *)(__IO void *)(&hcrc->Instance->DR) = *(__IO uint8_t *)(__IO void *)(&pBuffer[index]);
  }
  temp = hcrc->Instance->DR;
*/
  temp = crc32_handle8(hcrc, (uint8_t *)pBuffer, BufferLength);

  /* Change CRC peripheral state */
  hcrc->State = HAL_CRC_STATE_READY;

  /* Return the CRC computed value */
  //temp ^= 0xFFFFFFFF;
  return temp;
}

/**
  * @brief  Enter 8-bit input data to the CRC calculator.
  *         Specific data handling to optimize processing time.
  * @param  hcrc CRC handle
  * @param  pBuffer pointer to the input data buffer
  * @param  BufferLength input data buffer length
  * @retval uint32_t CRC (returned value LSBs for CRC shorter than 32 bits)
  */
static uint32_t crc32_handle8( CRC_HandleTypeDef *hcrc, uint8_t pBuffer[], uint32_t BufferLength )
{
  volatile uint16_t  data;
  volatile uint16_t  *pReg;
  volatile uint32_t  CRC32;
  uint32_t   byte1;
  uint32_t   byte2;
  uint32_t   byte3;
  uint32_t   byte4;
  uint32_t   crcInputWord;

  /* Processing time optimization: 4 bytes are entered in a row with a single word write,
   * last bytes must be carefully fed to the CRC calculator to ensure a correct type
   * handling by the peripheral */

    /* Processing time optimization: 4 bytes are entered in a row with a single word write,
   * last bytes must be carefully fed to the CRC calculator to ensure a correct type
   * handling by the peripheral */
  
  /* handle n * 4 byte chunks + 0 byte */
   if ((BufferLength % 4U) == 0U)
   {
      for (uint32_t i = 0U; i < (BufferLength / 4U); i++)
      {
         hcrc->Instance->DR = ((uint32_t)pBuffer[4U * i] << 24U) | \
                              ((uint32_t)pBuffer[(4U * i) + 1U] << 16U) | \
                              ((uint32_t)pBuffer[(4U * i) + 2U] << 8U)  | \
                              (uint32_t)pBuffer[(4U * i) + 3U];
      }
      CRC32 = hcrc->Instance->DR;
   }
   
   /* handle n * 4 byte chunks + 1 byte */
   if ((BufferLength % 4U) == 1U)
   {
      // fill the first 3 Bytes with FF's
      hcrc->Instance->DR = ((uint32_t)0xFF << 24U) | \
                           ((uint32_t)0xFF << 16U) | \
                           ((uint32_t)0xFF << 8U)  | \
                           (uint32_t)pBuffer[0];
      
      if(BufferLength==1)
      {
         // just invert the first 3 values and finish the crc computation
         uint8_t *temp;
         CRC32 = hcrc->Instance->DR;
         temp = (uint8_t*)&CRC32;
         temp[1] = ~temp[1];
         temp[2] = ~temp[2];
         temp[3] = ~temp[3];
      }
      else
      {
         // second byte
         byte1 = ((uint32_t)pBuffer[1U] << 24U);
         byte2 = ((uint32_t)pBuffer[2U] << 16U);
         byte3 = ((uint32_t)pBuffer[3U] << 8U);
         byte4 = (uint32_t)pBuffer[4U];
         
         byte1 = ~byte1;
         byte2 = ~byte2;
         byte3 = ~byte3;
         
         byte1 = byte1 & 0xFF000000;
         byte2 = byte2 & 0x00FF0000;
         byte3 = byte3 & 0x0000FF00;
            
         crcInputWord = byte1 | byte2 | byte3 | byte4;
            
         hcrc->Instance->DR = crcInputWord;
         
         // from th third byte on just feed the crc wordwise
         for (uint32_t i = 1U; i < (BufferLength / 4U); i++)
         {
            hcrc->Instance->DR = ((uint32_t)pBuffer[(4U * i) + 1U] << 24U) | \
                                 ((uint32_t)pBuffer[(4U * i) + 2U] << 16U) | \
                                 ((uint32_t)pBuffer[(4U * i) + 3U] << 8U)  | \
                                 (uint32_t)pBuffer[(4U * i) + 4U];
         }
         
         // read the final crc value
         CRC32 = hcrc->Instance->DR;
      }
   }
   
   /* handle n * 4 byte chunks + 2 byte */
   if ((BufferLength % 4U) == 2U)
   {
      // fill the first 2 Bytes with FF's
      hcrc->Instance->DR = ((uint32_t)0xFF << 24U) | \
                           ((uint32_t)0xFF << 16U) | \
                           ((uint32_t)pBuffer[0] << 8U)  | \
                           (uint32_t)pBuffer[1];
      
      if(BufferLength==2)
      {
         // just invert the first 2 values and finish the crc computation
         uint8_t *temp;
         CRC32 = hcrc->Instance->DR;
         temp = (uint8_t*)&CRC32;
         temp[2] = ~temp[2];
         temp[3] = ~temp[3];
      }
      else
      {
         // second byte
         byte1 = ((uint32_t)pBuffer[2U] << 24U);
         byte2 = ((uint32_t)pBuffer[3U] << 16U);
         byte3 = ((uint32_t)pBuffer[4U] << 8U);
         byte4 = (uint32_t)pBuffer[5U];
         
         byte1 = ~byte1;
         byte2 = ~byte2;
         
         byte1 = byte1 & 0xFF000000;
         byte2 = byte2 & 0x00FF0000;
            
         crcInputWord = byte1 | byte2 | byte3 | byte4;
            
         hcrc->Instance->DR = crcInputWord;
         
         // from th third byte on just feed the crc wordwise
         for (uint32_t i = 1U; i < (BufferLength / 4U); i++)
         {
            hcrc->Instance->DR = ((uint32_t)pBuffer[(4U * i) + 2U] << 24U) | \
                                 ((uint32_t)pBuffer[(4U * i) + 3U] << 16U) | \
                                 ((uint32_t)pBuffer[(4U * i) + 4U] << 8U)  | \
                                  (uint32_t)pBuffer[(4U * i) + 5U];
         }
         
         // read the final crc value
         CRC32 = hcrc->Instance->DR;
      }
   }
   
   /* handle n * 4 byte chunks + 3 byte */
   if ((BufferLength % 4U) == 3U)
   {
      // fill the first 1 Byte with FF
      hcrc->Instance->DR = ((uint32_t)0xFF << 24U) | \
                           ((uint32_t)pBuffer[0] << 16U) | \
                           ((uint32_t)pBuffer[1] << 8U)  | \
                           (uint32_t)pBuffer[2];
      
      if(BufferLength==3)
      {
         // just invert the first 2 values and finish the crc computation
         uint8_t *temp;
         CRC32 = hcrc->Instance->DR;
         temp = (uint8_t*)&CRC32;
         temp[3] = ~temp[3];
      }
      else
      {
         // second byte
         byte1 = ((uint32_t)pBuffer[3U] << 24U);
         byte2 = ((uint32_t)pBuffer[4U] << 16U);
         byte3 = ((uint32_t)pBuffer[5U] << 8U);
         byte4 = (uint32_t)pBuffer[6U];
         
         byte1 = ~byte1;
         
         byte1 = byte1 & 0xFF000000;
            
         crcInputWord = byte1 | byte2 | byte3 | byte4;
            
         hcrc->Instance->DR = crcInputWord;
         
         // from th third byte on just feed the crc wordwise
         for (uint32_t i = 1U; i < (BufferLength / 4U); i++)
         {
            hcrc->Instance->DR = ((uint32_t)pBuffer[(4U * i) + 3U] << 24U) | \
                                 ((uint32_t)pBuffer[(4U * i) + 4U] << 16U) | \
                                 ((uint32_t)pBuffer[(4U * i) + 5U] << 8U)  | \
                                  (uint32_t)pBuffer[(4U * i) + 6U];
         }
         
         // read the final crc value
         CRC32 = hcrc->Instance->DR;
      }
   }

  /* Return the CRC computed value */
  return CRC32;
}
/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/