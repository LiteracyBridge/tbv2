// TBookV2  fileOps.c   audio file decode or encrypt thread
//  Jul2020

#include "fileOps.h"
#include "EncAudio.h"
#include "controlmanager.h"     // buildPath
#include <string.h>

const int               FILE_ENCRYPT_REQ = 0x01;    // signal sent to request audio file encryption
const int               FILE_DECODE_REQ  = 0x02;    // signal sent to request audio file decoding
const int               FILEOP_EVENTS    = 0x3;     // mask for all events
const int               FILE_DECODE_DONE = 0x01;    // signal sent to report audio file decoding complete

static osThreadAttr_t   thread_attr;
static osEventFlagsId_t mFileOpEventId;             // for signals to fileOpThread

static volatile char    mFileArgPath[MAX_PATH];     // communication variable shared with fileOpThread

uint32_t                Mp3FilesToConvert;
char *                  mp3Name;     // mp3 being decoded
int                     mp3ErrCnt;
int                     mp3FirstErrLoc;
char *                  mp3FirstErrMsg;

static void fileOpThread( void *arg );            // forward

extern void initFileOps( void ) {                // init fileOps & spawn thread to handle audio encryption & decoding
    mFileOpEventId = osEventFlagsNew( NULL );         // osEvent channel for communication with mediaThread
    if ( mFileOpEventId == NULL )
        tbErr( "mFileOpEventId alloc failed" );

    mFileArgPath[0] = 0;

    thread_attr.name       = "FileOp";
    thread_attr.stack_size = FILEOP_STACK_SIZE;
    void *thread = osThreadNew( fileOpThread, NULL, &thread_attr );
    if ( thread == NULL )
        tbErr( "fileOpThread spawn failed" );
}

extern void decodeAudio() {                  // decode all .mp3 files in /system/audio/ & /package/*/  to .wav
    osEventFlagsSet( mFileOpEventId, FILE_DECODE_REQ );   // transfer request to fileOpThread
}

extern void copyEncrypted( char *src ) {     // make an encrypted copy of src (.wav) as dst
    strcpy((char *) mFileArgPath, src );
    osEventFlagsSet( mFileOpEventId, FILE_ENCRYPT_REQ );    // transfer request to fileOpThread
}

//
//****************** internal
static void encryptCopy() {
    char basenm[MAX_PATH];
    strcpy( basenm, (const char *) mFileArgPath );   // copy *.wav path
    char *pdot = strrchr( basenm, '.' );
    *pdot = 0;

    startEncrypt( basenm );       // write basenm.key & initialize AES encryption
    strcat( basenm, ".dat" );


    FILE *fwav = tbOpenReadBinary((const char *) mFileArgPath ); //fopen( (const char *)mFileArgPath, "rb" );
    FILE *fdat = tbOpenWriteBinary( basenm ); //fopen( basenm, "wb" );
    if ( fwav == NULL || fdat == NULL ) tbErr( "encryptCopy: fwav=%x fdat=%x", fwav, fdat );

    uint32_t flen     = 0;
    uint8_t  *wavbuff = tbAlloc( BUFF_SIZ, "wavbuff" );
    uint8_t  *datbuff = tbAlloc( BUFF_SIZ, "datbuff" );

    while (1) {
        uint16_t cnt = fread( wavbuff, 1, BUFF_SIZ, fwav );
        if ( cnt == 0 ) break;

        if ( cnt < BUFF_SIZ ) memset( wavbuff + cnt, 0, BUFF_SIZ - cnt );   // extend final block with 0s
        encryptBlock( wavbuff, datbuff, BUFF_SIZ );
        cnt = fwrite( datbuff, 1, BUFF_SIZ, fdat );
        flen += cnt;
        if ( cnt != BUFF_SIZ ) tbErr( "encryptCopy fwrite => %d", cnt );
    }
    tbFclose( fwav );    //int res = fclose( fwav );
    //if ( res != fsOK ) tbErr("wav fclose => %d", res );

    tbFclose( fdat );    //res = fclose( fdat );
    //if ( res != fsOK ) tbErr("dat fclose => %d", res );
    logEvtNS( "encF", "dat", basenm );

    basenm[strlen( basenm ) - 3] = 'x';
    char *newnm = strrchr( basenm, '/' ) + 1;
    //  res = frename( (const char *)mFileArgPath, newnm ); // rename .wav => .xat
    int res = fdelete((const char *) mFileArgPath, NULL );    // delete wav file
    if ( res != fsOK ) tbErr( "dat fclose => %d", res );
    FileSysPower( false );  // powerdown SDIO after encrypting recording
}

/*static void         decodeMp3( char *fpath ){     // decode fname (.mp3) & copy to .wav
  int flen = strlen( fpath );
  fpath[ flen-4 ] = 0;
  strcat( fpath, ".wav" );
  if ( !fexists( fpath )){
    fpath[ flen-4 ] = 0;
    strcat( fpath, ".mp3" );
    mp3ToWav( fpath );
        showProgress( "GRG__", 400 );      // decode MP3's
  } else 
    dbgLog( "7 %s already decoded\n", fpath );
}
static bool         endsWith( char *nm, char *sfx ){
  int nmlen = strlen( nm ), slen = strlen( sfx );
  if ( nmlen < slen ) return false;
  return strcasecmp( nm+nmlen-slen, sfx )==0;
}
*/

static void scanDirForMp3s( char *dir, bool count ) {  // scan dir for .mp3 without .wav & decode
    fsFileInfo fInfo;
    fInfo.fileID = 0;
    char patt[100], path[100];

    strcpy( patt, dir );
    strcat( patt, "*.mp3" );    // pattern to search for .mp3 files

    while (ffind( patt, &fInfo ) == fsOK) {    // scan mp3s in dir
        strcpy( path, dir );
        strcat( path, fInfo.name );      // "dir/file.mp3"

        int dlen = strlen( path );
        path[dlen - 4] = 0;                 // "dir/file"
        strcat( path, ".wav" );         // "dir/file.wav"
        if ( !fexists( path )) {
            if ( count ) {  // 1st pass -- count unconverted files
                Mp3FilesToConvert++;
            } else {     // 2nd pass -- do the conversions
                path[dlen - 4] = 0;
                strcat( path, ".mp3" );
                logEvtNSNI( "decodeMP3", "nm", fInfo.name, "Sz", fInfo.size );
                mp3Name   = fInfo.name;
                mp3ErrCnt = 0;
                mp3ToWav( path );      // convert the file
                if ( mp3ErrCnt > 0 ) {
                    logEvtNSNININS( "mp3Err", "nm", mp3Name, "errCnt", mp3ErrCnt, "pos", mp3FirstErrLoc, "msg", mp3FirstErrMsg );
                }
                Mp3FilesToConvert--;
                mp3Name = 0;
                showProgress( "R_", 200 );   //  decode MP3's
            }
        }
    }
}

static void scanDecodeAudio( bool count ) {      // scan Deployment->AudioPaths->AudPath[0..nPaths] for .mp3 & copy to .wav
    int      nP = Deployment->AudioPaths->nPaths;
    for (int i  = 0; i < nP; i++)
        scanDirForMp3s( Deployment->AudioPaths->audPath[i], count );
    endProgress();
}

static void fileOpThread( void *arg ) {
    while (true) {
        uint32_t flags = osEventFlagsWait( mFileOpEventId, FILEOP_EVENTS, osFlagsWaitAny, osWaitForever );

        if (( flags & FILE_ENCRYPT_REQ ) != 0 ) {
            encryptCopy();     // only requested if PrivateFeedback == True
        }

        if (( flags & FILE_DECODE_REQ ) != 0 ) {
            Mp3FilesToConvert = 1;          // start at 1 & decrement at end of counting pass (so >0 until all are converted)
            scanDecodeAudio( true );  // pass to count .mp3's without .wavs
            Mp3FilesToConvert--;   // counting pass complete  -- decrement so only # remaining

            if ( Mp3FilesToConvert > 0 ) {
                logEvtNI( "Convert MP3s", "Cnt", Mp3FilesToConvert );
                scanDecodeAudio( false );   // this time do the conversions
                logEvt( " Mp3's converted" );
            }
            osEventFlagsSet( mFileOpSignal, FILE_DECODE_DONE );   // signal controlManager to proceed
        }
    }
}
