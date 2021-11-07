// ****************************************************************************
/// \file      crc32.h
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

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CRC32_H
#define __CRC32_H

// Exported defines ***********************************************************

// Exported types *************************************************************

// Exported functions *********************************************************
void        crc32_init        ( void );
void        crc32_deinit      ( void );
uint32_t    crc32_calculate   (CRC_HandleTypeDef *hcrc, uint32_t pBuffer[], uint32_t BufferLength);

#endif /* __CRC32_H */

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/