// TBookV2  fileOps.c		audio file decode or encrypt thread
//  Jul2020

#include "fileOps.h"
#include "EncAudio.h"
#include "controlmanager.h"			// buildPath
#include <string.h>

const int 									FILE_ENCRYPT_REQ   =	0x01; 	// signal sent to request audio file encryption
const int 									FILE_DECODE_REQ	 	 =  0x02;		// signal sent to request audio file decoding
const int 									FILEOP_EVENTS 		 =  0x3;		// mask for all events

static 	osThreadAttr_t 			thread_attr;
static  osEventFlagsId_t		mFileOpEventId;								// for signals to fileOpThread

static volatile char				mFileArgPath[ MAX_PATH ];			// communication variable shared with fileOpThread


static void 	fileOpThread( void *arg );						// forward 
	
extern void					initFileOps( void ){								// init fileOps & spawn thread to handle audio encryption & decoding
	mFileOpEventId = osEventFlagsNew(NULL);					// osEvent channel for communication with mediaThread
	if ( mFileOpEventId == NULL)
		tbErr( "mFileOpEventId alloc failed" );	
	
	mFileArgPath[0] = 0;			
	
	thread_attr.name = "FileOp";
	thread_attr.stack_size = FILEOP_STACK_SIZE; 
	void * thread = osThreadNew( fileOpThread, NULL, &thread_attr );
	if ( thread == NULL )
		tbErr( "fileOpThread spawn failed" );	
}
extern void 				decodeAudio(){									// decode all .mp3 files in /system/audio/ & /package/*/  to .wav 
	osEventFlagsSet( mFileOpEventId, FILE_DECODE_REQ );		// transfer request to fileOpThread
}

extern void					copyEncrypted( char *src ){			// make an encrypted copy of src (.wav) as dst
	strcpy( (char *)mFileArgPath, src );
	osEventFlagsSet( mFileOpEventId, FILE_ENCRYPT_REQ );		// transfer request to fileOpThread
}
//
//****************** internal
static void 				encryptCopy( ){
	char basenm[ MAX_PATH ];
	strcpy( basenm, (const char *)mFileArgPath );		// copy *.wav path
	char *pdot = strrchr( basenm, '.' );
	*pdot = 0;
	
 	startEncrypt( basenm );				// write basenm.key & initialize AES encryption
	strcat( basenm, ".dat" );
	

	FILE * fwav = tbOpenReadBinary( (const char *)mFileArgPath ); //fopen( (const char *)mFileArgPath, "rb" );
	FILE * fdat = tbOpenWriteBinary( basenm ); //fopen( basenm, "wb" );
	if ( fwav==NULL || fdat==NULL ) tbErr("encryptCopy: fwav=%x fdat=%x", fwav, fdat );

	uint32_t flen = 0;
	uint8_t *wavbuff = tbAlloc( BUFF_SIZ, "wavbuff");
	uint8_t *datbuff = tbAlloc( BUFF_SIZ, "datbuff");
	
	while (1){
		uint16_t cnt = fread( wavbuff, 1, BUFF_SIZ, fwav );
		if ( cnt == 0 ) break;
		
		if ( cnt < BUFF_SIZ ) memset( wavbuff+cnt, 0, BUFF_SIZ-cnt );		// extend final block with 0s
		encryptBlock( wavbuff, datbuff, BUFF_SIZ );
		cnt = fwrite( datbuff, 1, BUFF_SIZ, fdat );
    flen += cnt;
		if (cnt != BUFF_SIZ ) tbErr("encryptCopy fwrite => %d", cnt);
 	}
	tbCloseFile( fwav );		//int res = fclose( fwav );
	//if ( res != fsOK ) tbErr("wav fclose => %d", res );
	
	tbCloseFile( fdat );		//res = fclose( fdat );
	//if ( res != fsOK ) tbErr("dat fclose => %d", res );
	logEvtNS("encF", "dat", basenm );

  basenm[ strlen(basenm)-3 ] = 'x';
	char * newnm = strrchr( basenm, '/' )+1;
// 	res = frename( (const char *)mFileArgPath, newnm );	// rename .wav => .xat
 	int res = fdelete( (const char *)mFileArgPath, NULL );		// delete wav file
	if ( res != fsOK ) tbErr("dat fclose => %d", res );	
	FileSysPower( false );  // powerdown SDIO after encrypting recording
}

static void 				decodeMp3( char *fpath ){			// decode fname (.mp3) & copy to .wav
	int flen = strlen( fpath );
	fpath[ flen-4 ] = 0;
	strcat( fpath, ".wav" );
	if ( !fexists( fpath )){
		fpath[ flen-4 ] = 0;
		strcat( fpath, ".mp3" );
		mp3ToWav( fpath );
	} else 
	  dbgLog( "7 %s already decoded\n", fpath );
}
static bool					endsWith( char *nm, char *sfx ){
	int nmlen = strlen( nm ), slen = strlen( sfx );
	if ( nmlen < slen ) return false;
	return strcasecmp( nm+nmlen-slen, sfx )==0;
}
static void 				scanTree( char *dstpath ){			// scan & decode mp3s in 'path'
	fsFileInfo fInfo;
	fInfo.fileID = 0;
	
	int dlen = strlen( dstpath );
	if ( dstpath[dlen-1]!='/' ) 
		strcat( dstpath, "/" ); 
	strcat( dstpath, "*" );
	dlen = strlen( dstpath );
	
	while ( ffind( dstpath, &fInfo )==fsOK ){		// scan everything in dir 
		
		dstpath[ dlen-1 ] = 0;		// overwrite "*"
		strcat( dstpath, fInfo.name );
		
		if ( fInfo.attrib == 0x10 &&  fInfo.name[0]!='.' ){  // dir other than . or ..
			scanTree( dstpath );
		} else if ( endsWith( fInfo.name, ".MP3" ))
			decodeMp3( dstpath );
		
		dstpath[ dlen-1 ] = 0;		// revert to "path/*"
		strcat( dstpath, "*" );
	}
}
static void 				scanDecodeAudio( ){			// scan audio paths for .mp3 & copy to .wav
	char path[ MAX_PATH ];
	strcpy( path, "M0:/" );
	scanTree( path );
}

static void 				fileOpThread( void *arg ){				

	if ( FirstSysBoot )	// on first boot, scan for audio to decode
		scanDecodeAudio();
	
	while (true){		
		uint32_t flags = osEventFlagsWait( mFileOpEventId, FILEOP_EVENTS,  osFlagsWaitAny, osWaitForever );
		
		if ( (flags & FILE_ENCRYPT_REQ) != 0 ){
			encryptCopy();
		}
		
		if ( (flags & FILE_DECODE_REQ) != 0 ){
			scanDecodeAudio();
		}
	}
}
