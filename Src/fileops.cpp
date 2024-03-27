// TBookV2  fileOps.c   audio file decode or encrypt thread
//  Jul2020

#include <string.h>

#include "fileOps.h"
#include "encAudio.h"
#include "controlmanager.h"
#include "mp3tostream.h"

//#define BUFF_SIZ 512

//const int               FILE_ENCRYPT_REQ = 0x01;    // signal sent to request audio file encryption
//const int               FILE_DECODE_REQ  = 0x02;    // signal sent to request audio file decoding
//const int               FILEOP_EVENTS    = 0x3;     // mask for all events
//const int               FILE_DECODE_DONE = 0x01;    // signal sent to report audio file decoding complete

//static osThreadAttr_t   thread_attr;
//static osEventFlagsId_t mFileOpEventId;             // for signals to fileOpThread

//static volatile char    mFileArgPath[MAX_PATH];     // communication variable shared with fileOpThread

//uint32_t                Mp3FilesToConvert;
//char *                  mp3Name;     // mp3 being decoded
//int                     mp3ErrCnt;
//int                     mp3FirstErrLoc;
//char *                  mp3FirstErrMsg;

osEventFlagsId_t fileOpRequestEventId;
osEventFlagsId_t fileOpResultEventId;
osMutexId_t fileOpMutex;




//static void fileOpThread( void *arg );            // forward
void fileOpThreadFunc(void *args);

extern void fileOpInit( void ) {                // init fileOps & spawn thread to handle audio encryption & decoding
    fileOpMutex = osMutexNew(NULL);
    fileOpRequestEventId = osEventFlagsNew (NULL );
    fileOpResultEventId = osEventFlagsNew (NULL );

    // Keil documentation is silent on the required lifetime of this struct.
    // The examples do show sharing this between multiple concurrent threads.
    static osThreadAttr_t threadAttr = {
            .name = "Mp3Stream",
            .priority = osPriorityAboveNormal1 // osPriorityHigh
    };
    threadAttr.stack_size = 10000; //FILEOP_STACK_SIZE;
    osThreadId_t decoderThread = osThreadNew(fileOpThreadFunc, NULL, &threadAttr);
    dbgLog("4 fileopThread: %x\n", decoderThread);
    if (decoderThread == NULL) {
        tbErr("fileopThread spawn failed");
    }

    // There should be no reason we need to supply the mq memory, but without this, for some unknowable
    // reason, opening a FILE with one already open causes an osRtxErrorClibMutex error.
    osMessageQueueAttr_t messageQueueAttr;
    messageQueueAttr.name="encryptq";
    messageQueueAttr.mq_mem = encryptCb.mq_mem;
    messageQueueAttr.mq_size = sizeof(encryptCb.mq_mem);
    encryptCb.encryptQueueId = osMessageQueueNew(ENCRYPT_QUEUE_LENGTH, sizeof(Buffer_t *), &messageQueueAttr);
}

/**
 * Thread proc for mp3 streamer.
 * @param args arguments provided to the thread.
 */
void fileOpThreadFunc(void *args) {
    dbgLog( "4 fileOpThread: 0x%x 0x%x \n", &args, &args - FILEOP_STACK_SIZE );
    int32_t waitFlags = kDecodeRequest | kEncryptPrepareRequest;
    while (true) {
        uint32_t eventFlags = osEventFlagsWait(fileOpRequestEventId, waitFlags, osFlagsWaitAny, osWaitForever);

        if (eventFlags & kDecodeRequest) {
            streamMp3DecodeLoop();
        } else if (eventFlags & kEncryptPrepareRequest) {
            encUfAudioLoop();
        }
    }
}


//static void fileOpThread( void *arg ) {
//    while (true) {
//        uint32_t flags = osEventFlagsWait( mFileOpEventId, FILEOP_EVENTS, osFlagsWaitAny, osWaitForever );

//        if (( flags & FILE_ENCRYPT_REQ ) != 0 ) {
//            encryptCopy();     // only requested if PrivateFeedback == True
//        }

//        if (( flags & FILE_DECODE_REQ ) != 0 ) {
//            Mp3FilesToConvert = 1;          // start at 1 & decrement at end of counting pass (so >0 until all are converted)
//            scanDecodeAudio( true );  // pass to count .mp3's without .wavs
//            Mp3FilesToConvert--;   // counting pass complete  -- decrement so only # remaining

//            if ( Mp3FilesToConvert > 0 ) {
//                logEvtNI( "Convert MP3s", "Cnt", Mp3FilesToConvert );
//                scanDecodeAudio( false );   // this time do the conversions
//                logEvt( " Mp3's converted" );
//            }
//            osEventFlagsSet( mFileOpSignal, FILE_DECODE_DONE );   // signal controlManager to proceed
//        }
//    }
//}
