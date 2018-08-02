/*
 * MP3Decoder.h
 *
 *  Created on: May 12, 2016
 *      Author: Michael
 */

#ifndef MP3DECODER_H_
#define MP3DECODER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "diag/Trace.h"
#include "fatfs.h"
#include "usb_host.h"
#include "stm32f4xx_hal.h"
#include "mp3dec.h"


#define	FILE_READ_BUFFER_SIZE	2000
#define MAX_FRAME				2304
extern	MP3FrameInfo			mp3FrameInfo;
extern	HMP3Decoder				hMP3Decoder;

/*
 * fill_mp3_buffer(FIL *fp, char *buffer, unsigned int read_length, bool reset_flag)
 * 		fp			File pointer
 * 		buffer		Buffer to hold audio samples
 * 		read_length	Number of bytes to read
 * 		reset_flag	True to reset decoding
 */
uint fill_mp3_buffer(FIL *fp, uint16_t * buffer, uint read_length, bool reset_flag);
uint32_t Mp3ReadId3V2Tag(FIL* pInFile, char* pszArtist, uint32_t unArtistSize, char* pszTitle, uint32_t unTitleSize);

#endif /* MP3DECODER_H_ */
