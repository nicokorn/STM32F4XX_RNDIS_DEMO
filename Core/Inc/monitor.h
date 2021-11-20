// ****************************************************************************
/// \file      monitor.h
///
/// \brief     Monitor C Header File
///
/// \details   This module contents actions to show the physical status of the
///			   board like temperature and input voltage.
///
/// \author    Nico Korn
///
/// \version   0.3.0.2
///
/// \date      14112021
/// 
/// \copyright Copyright (C) 2021  by "Reichle & De-Massari AG", 
///            all rights reserved.
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

// Define to prevent recursive inclusion **************************************
#ifndef __MONITOR_H
#define __MONITOR_H

// Include ********************************************************************
#include "stm32f4xx_hal.h"

// Exported defines ***********************************************************

// Exported types *************************************************************

// Functions ******************************************************************
void          monitor_init                ( void );
void          monitor_deinit              ( void );
float         monitor_getTemperature      ( void );
float         monitor_getVoltage          ( void );
#endif // __MONITOR_H