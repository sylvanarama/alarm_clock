/*
 * PlayMP3.h
 *
 *  Created on: May 12, 2016
 *      Author: Michael
 */

#ifndef INCLUDE_PLAYMP3_H_
#define INCLUDE_PLAYMP3_H_

#include <stdio.h>
#include <stdlib.h>
#include "diag/Trace.h"
#include "fatfs.h"
#include "usb_host.h"
#include "stm32f4xx_hal.h"
#include "MP3Decoder.h"



extern DAC_HandleTypeDef
	AudioDac;


#define		NUMBER_BUFFER	2

/*
 * PlayDirectory(char *drive)
 * Plays all MP3 files in the specified directory.
 */

int PlayDirectory(const char* path, unsigned char seek);
void PlayMP3File(const char *fname);

const char *get_filename_ext(const char *filename);

#endif /* INCLUDE_PLAYMP3_H_ */
