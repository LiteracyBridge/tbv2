#ifndef FILEOPS_H
#define FILEOPS_H

#include "tbook.h"

extern osEventFlagsId_t	    mFileOpSignal;
extern const int 			FILE_DECODE_DONE;		// signal sent to report audio file decoding complete
extern char                 * mp3Name;
extern int                  mp3ErrCnt;
extern int                  mp3FirstErrLoc;
extern char                 * mp3FirstErrMsg;

extern void					initFileOps( void );								// init fileOps & spawn thread to handle audio encryption & decoding
extern void 				decodeAudio( void );								// decode all .mp3 files in /system/audio/ & /package/*/  to .wav 
extern void					copyEncrypted( char * src );				// make an encrypted copy of src (.wav) as dst
extern void 				mp3ToWav( const char *nm );					// decode nm.mp3 to nm.wav
#endif
