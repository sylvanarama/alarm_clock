/*
 * mono.h
 *
 *  Created on: Feb 22, 2017
 *      Author: brent
 */

#ifndef INCLUDE_MONO_H_
#define INCLUDE_MONO_H_

#include <generic.h>

#define ANALOG_TO_MONO_RIGHT_CHANNEL 0
#define ANALOG_TO_MONO_LEFT_CHANNEL 1
#define ANALOG_TO_MONO_AVERAGE_CHANNELS 2

#define ANALOG_TO_MONO_STATUS_OK 0
#define ANALOG_TO_MONO_STATUS_INVALID_MERGE_TYPE -1
#define ANALOG_TO_MONO_STATUS_TOO_MANY_CHANNELS -2

int16_t AudioToMono( int16_t *Stream, uint16_t StreamSize, uint16_t MergeType, uint16_t NumberOfChannels, int16_t *ReturnStatus );



#endif /* INCLUDE_MONO_H_ */
