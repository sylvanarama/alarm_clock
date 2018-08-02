/*
 * DebugPort.h
 *
 *  Created on: Mar 22, 2017
 *      Author: brent
 */

#ifndef INCLUDE_DEBUGPORT_H_
#define INCLUDE_DEBUGPORT_H_

//
// Set to true if you want trace_printf data to be sent via the virtual communications port
//
// *** Note *** You need to make a modification to the stm32f4discovery board to use the virtual communications port
//

#define DIAGNOSTIC_PORT_ENABLED (TRUE)

void DebugPortInit( void );
int DebugWrite( const char *ptr, int len );


#endif /* INCLUDE_DEBUGPORT_H_ */
