// ****************************************************************************
/// \file      dhcp server
///
/// \brief     dhcp server header file
///
/// \details   
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

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx.h"

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DHCPSERVER_H
#define __DHCPSERVER_H

// Exported defines ***********************************************************

// Exported types *************************************************************
//typedef struct leaseobj_s
//{
//}leaseobj_t;

typedef struct leasetable_s
{
	uint8_t  mac[6];
	uint8_t  ip[4];
	uint8_t  subnet[4];
	uint32_t leasetime;
} leasetableObj_t;

typedef struct dhcpconf_s
{
	uint8_t           dhcpip[4];
	uint8_t           dns[4];
	uint8_t           sub[4];
	const char        *domain;
	uint16_t          tablelength;
	leasetableObj_t   *leasetable;
} dhcpconf_t;

// Exported functions *********************************************************
void dhcpserver_init    ( dhcpconf_t *dhcpconf_param );
void dhcpserver_deinit  ( void );

#endif /* __DHCPSERVER_H */

/********************** (C) COPYRIGHT Reichle & De-Massari *****END OF FILE****/