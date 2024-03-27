/*------------------------------------------------------------------------------
 USBD_User_Device_MSC_LUN.c
 TalkingBook -- define callbacks for USBD_MSC library operations
 
 Overrides WEAK definitions from usbd_config.h
__WEAK  void              USBD_Device0_Initialize                    (void)                                                               { }
__WEAK  void              USBD_Device0_Uninitialize                  (void)                                                               { }
__WEAK  void              USBD_Device0_VbusChanged                   (bool level)                                                         { (void)level; }
extern  void              USBD_Device0_EventReset                    (void);
__WEAK  void              USBD_Device0_EventReset                    (void)                                         { }   // Deprecated  
__WEAK  void              USBD_Device0_Reset                         (void)                                                               { USBD_Device0_EventReset (); }
__WEAK  void              USBD_Device0_HighSpeedActivated            (void)                                                               { }
__WEAK  void              USBD_Device0_Suspended                     (void)                                                               { }
__WEAK  void              USBD_Device0_Resumed                       (void)                                                               { }
__WEAK  void              USBD_Device0_ConfigurationChanged          (uint8_t val)                                                        { (void)val; }
__WEAK  void              USBD_Device0_EnableRemoteWakeup            (void)                                                               { }
__WEAK  void              USBD_Device0_DisableRemoteWakeup           (void)                                                               { }
__WEAK  usbdRequestStatus USBD_Device0_Endpoint0_SetupPacketReceived (const USB_SETUP_PACKET *setup_packet, uint8_t **buf, uint32_t *len) { (void)setup_packet; (void)buf; (void)len; return usbdRequestNotProcessed; }
__WEAK  void              USBD_Device0_Endpoint0_SetupPacketProcessed(const USB_SETUP_PACKET *setup_packet)                               { (void)setup_packet;                                                       }
__WEAK  usbdRequestStatus USBD_Device0_Endpoint0_OutDataReceived     (uint32_t len)                                                       { (void)len; return usbdRequestNotProcessed; }
__WEAK  usbdRequestStatus USBD_Device0_Endpoint0_InDataSent          (uint32_t len)                                                       { (void)len; return usbdRequestNotProcessed; }
 
__WEAK  void      USBD_MSC0_Initialize                    (void)                                                        { }
__WEAK  void      USBD_MSC0_Uninitialize                  (void)                                                        { }
__WEAK  bool      USBD_MSC0_GetCacheInfo                  (uint32_t *buffer, uint32_t *size)                            { (void)buffer; (void) size; return false; }
__WEAK  bool      USBD_MSC0_GetMediaCapacity              (uint32_t *block_count, uint32_t *block_size)                 { (void)block_count; (void)block_size; return false; }
__WEAK  bool      USBD_MSC0_Read                          (uint32_t lba, uint32_t cnt,       uint8_t *buf)              { (void)lba; (void)cnt; (void)buf; return false; }
__WEAK  bool      USBD_MSC0_Write                         (uint32_t lba, uint32_t cnt, const uint8_t *buf)              { (void)lba; (void)cnt; (void)buf; return false; }
__WEAK  bool      USBD_MSC0_StartStop                     (bool start)                                                  { (void)start; return false; }
__WEAK  uint32_t  USBD_MSC0_CheckMedia                    (void)                                                        { return USBD_MSC_MEDIA_READY; }
__WEAK  uint8_t   USBD_MSC0_GetMaxLUN                     (void)                                                        { return 0U; }

__WEAK  bool      USBD_MSC0_LUN_GetMediaCapacity          (uint8_t lun, uint32_t *block_count, uint32_t *block_size)    { (void)lun; return (USBD_MSC0_GetMediaCapacity (block_count, block_size)); }
__WEAK  bool      USBD_MSC0_LUN_Read                      (uint8_t lun, uint32_t lba, uint32_t cnt,       uint8_t *buf) { (void)lun; return (USBD_MSC0_Read             (lba, cnt, buf));           }
__WEAK  bool      USBD_MSC0_LUN_Write                     (uint8_t lun, uint32_t lba, uint32_t cnt, const uint8_t *buf) { (void)lun; return (USBD_MSC0_Write            (lba, cnt, buf));           }
__WEAK  bool      USBD_MSC0_LUN_StartStop                 (uint8_t lun, bool start)                                     { (void)lun; return (USBD_MSC0_StartStop        (start));                   }
__WEAK  uint32_t  USBD_MSC0_LUN_CheckMedia                (uint8_t lun)                                                 { (void)lun; return (USBD_MSC0_CheckMedia       ());                        }

 */

#include "tb_evr.h"     // EVR codes for tbook
#include "rl_fs.h"
#include "rl_usb.h"
#include "tbook.h"

//static char  ser_no_string_desc[32];  // String Descriptor: LEN TYP + 4 + 3*8
//const int maxLUNs = 4;
#define maxLUNs  4
static struct LogicalDrive {
    char    drive[6];       // "M0:" or "M1:" or "N0:" or "F0:"
    int32_t drv_id;         // FAT drive id
    bool    ownedByUSB;     // USB MSCn media ownership
    bool    media_ok;       // Media is initialized and ok

}          LUN[maxLUNs];
static int nLUNs = 0;

volatile bool usbProvidingMassStorage = false;  // flag indicating USB has control of FileSys as MSC
//volatile  bool                usbRequestChange        = false;
//volatile  bool                usbRequestMSC           = false;
static bool   usbIsInitialized        = false;

void initLUN( uint8_t lun ) {         // init LUN[ lun ].drive & lock for USB access
    uint32_t param;
    if ( lun >= nLUNs ) return;
    LUN[lun].media_ok = false;

    int stat = finit( LUN[lun].drive );
    dbgEvt( TB_usbLnInit, lun, stat, 0, 0 );
    if ( stat != fsOK ) return;                   // failed to init

    stat = funmount( LUN[lun].drive );          // unmount from TBook
    dbgEvt( TB_usbLnUnm, lun, stat, 0, 0 );

    LUN[lun].ownedByUSB = true;
    int32_t drv_id = fs_ioc_get_id( LUN[lun].drive );   // Get ID of media drive
    dbgEvt( TB_usbDrvId, lun, drv_id, 0, 0 );
    if ( drv_id < 0 ) return;                   // invalid => fail

    LUN[lun].drv_id = drv_id;

    param = FS_CONTROL_MEDIA_INIT;     // Parameter for fsDevCtrlCodeControlMedia 0: Initialize media
    stat  = fs_ioc_device_ctrl( drv_id, fsDevCtrlCodeControlMedia, &param );
    dbgEvt( TB_usbLnIMed, lun, stat, 0, 0 );
    if ( stat != fsOK ) return;          // return if failed

    stat = fs_ioc_lock( drv_id );         // Lock media for USB usage
    if ( stat != fsOK ) return;          // return if failed

    LUN[lun].media_ok = true;           // success -- mark as ok
    dbgEvt( TB_usbLnOk, lun, 0, 0, 0 );
}

void uninitLUN( uint8_t lun ) {       // release LUN[ lun ].drive for TBook access
    if ( !LUN[lun].media_ok ) return;             // not locked
    LUN[lun].media_ok = false;
    int stat          = fs_ioc_unlock( LUN[lun].drv_id );  // unlock media for USB usage

    stat = fmount( LUN[lun].drive );      // remount for use by TBook
    dbgEvt( TB_usbLnUninit, lun, 0, 0, 0 );
}

void addLUN( const char *drv ) {            // add drive name ( e.g. "M0:") to LUN[]
    if ( drv == NULL ) return;
    int dlen = strlen( drv );
    if ( dlen == 0 || dlen > 5 ) return;

    strcpy( LUN[nLUNs].drive, drv );    // copy drive name into LUN[ nLUNs ]
    nLUNs++;
}

//
//
//************************   export to TBook
bool isMassStorageEnabled( void ) {             // => true if usb is providing FileSys as MSC
    return usbProvidingMassStorage;
}

bool enableMassStorage( const char *drv0, const char *drv1, const char *drv2, const char *drv3 ) {  // init drives as MSC Logical Units 0..3
    if ( usbProvidingMassStorage ) return true;
    LedManager::ledFg( NULL );
    if ( !haveUSBpower())
        LedManager::ledFg( tbConfig.fgNoUSBcable ); // no USB cable!
    else
        LedManager::ledFg( tbConfig.fgUSBconnect );

    // initialize LUN[] array for specified drives
    addLUN( drv0 == NULL ? "M0:" : drv0 );
    addLUN( drv1 );
    addLUN( drv2 );
    addLUN( drv3 );
    dbgEvt( TB_usbEnMSC, nLUNs, 0, 0, 0 );

    usbStatus stat = USBD_Initialize( 0 );            // initialize -- calls USB_MSC0_Initialize which unmounts LUNs & locks for USB
    usbIsInitialized = ( stat == usbOK );
    if ( !usbIsInitialized ) return false;     // USB failed to initialize

    dbgEvt( TB_usbConn, 0, 0, 0, 0 );

    stat = USBD_Connect( 0 );                         // signal connection to Host, so Host will enumerate & discover configured drives
    return usbProvidingMassStorage;
}

bool disableMassStorage( void ) {               // disable USB MSC & return devices to File System
    if ( usbProvidingMassStorage ) {
        usbProvidingMassStorage = false;
        for (int i              = 0; i < nLUNs; i++)
            uninitLUN( i );       // unlock & remount for TBook

        dbgEvt( TB_usbDisMSC, 0, 0, 0, 0 );
        USBD_Disconnect( 0 );
        USBD_Uninitialize( 0 );
        usbIsInitialized        = false;
        usbProvidingMassStorage = false;
    }
    LedManager::ledFg( NULL );
    return usbProvidingMassStorage;
}

//
//
//************************   USB_User_Device callbacks to hook into middleware (override weak definitions from USB_thread )
void USBD_Device0_Initialize( void ) {                   // Callback from USBD_Initialize
    //  EventRecorderDisable( EventRecordAll,   EvtFsCore_No,   EvtFsMcSPI_No );    //FS & USB off
    dbgEvt( TB_usbInit, 0, 0, 0, 0 );
}

void USBD_Device0_Uninitialize( void ) {                 // Callback from USBD_Uninitialize
    dbgEvt( TB_usbUninit, 0, 0, 0, 0 );
}

void USBD_Device0_VbusChanged( bool level ) {            // Callback when VBUS level changes- true: VBUS level is high
    dbgEvt( TB_usbVbchg, level, 0, 0, 0 );
}

void USBD_Device0_Reset( void ) {                        // Callback upon USB Bus Reset signaling
    //  EventRecorderDisable( EventRecordAll,   EvtFsCore_No,   EvtFsMcSPI_No );    //FS & USB off
    dbgEvt( TB_usbReset, 0, 0, 0, 0 );
}

void USBD_Device0_HighSpeedActivated( void ) {           // Callback when USB Bus speed was changed to high-speed
    dbgEvt( TB_usbHSAct, 0, 0, 0, 0 );
}

void USBD_Device0_Suspended( void ) {                    // Callback when USB Bus goes into suspend mode (no bus activity for 3 ms)
    dbgEvt( TB_usbSusp, 0, 0, 0, 0 );
}

void USBD_Device0_Resumed( void ) {                      // Callback  when USB Bus activity resumes
    dbgEvt( TB_usbResum, 0, 0, 0, 0 );
}

void USBD_Device0_ConfigurationChanged( uint8_t val ) {  // callback when  Device was successfully enumerated- value > 0: active configuration
    dbgEvt( TB_usbCfgCh, val, 0, 0, 0 );
}

void USBD_Device0_EnableRemoteWakeup( void ) {           // Callback when Set Feature for Remote Wakeup comes over Control Endpoint 0
    dbgEvt( TB_usbRmtWkup, 0, 0, 0, 0 );
}

void USBD_Device0_DisableRemoteWakeup( void ) {          // Callback when Clear Feature for Remote Wakeup comes over Control Endpoint 0
    dbgEvt( TB_usbDisRemWk, 0, 0, 0, 0 );
}

usbdRequestStatus USBD_Device0_Endpoint0_SetupPacketReceived( const USB_SETUP_PACKET *setup_packet, uint8_t **buf, uint32_t *len ) {                                                       // Callback when Device 0 received SETUP PACKET on Control Endpoint 0
    uint16_t *suPkt = (uint16_t *) setup_packet;
    uint32_t suWd0  = ((uint32_t *) setup_packet )[0];
    dbgEvt( TB_usbSetup, suWd0, suWd0, suPkt[2],
            suPkt[3] ); //((char *)setup_packet)[0], setup_packet->bRequest, setup_packet->wValue, setup_packet->wIndex );

    return usbdRequestNotProcessed;
    /*  switch (setup_packet->bmRequestType.Type) {
        case USB_REQUEST_STANDARD:
          // Catch Get String Descriptor request for serial number string and return desired string:
          if ((setup_packet->bmRequestType.Dir       == USB_REQUEST_DEVICE_TO_HOST) &&      // Request to get
              (setup_packet->bmRequestType.Recipient == USB_REQUEST_TO_DEVICE     ) &&      // from device
              (setup_packet->bRequest                == USB_REQUEST_GET_DESCRIPTOR) &&      // the descriptor
             ((setup_packet->wValue >> 8)            == USB_STRING_DESCRIPTOR_TYPE) &&      // String Descriptor Type
             ((setup_packet->wValue & 0xFFU)         == 0x03U                     ) &&      // Index of String = 3
              (setup_packet->wIndex                  == 0x0409U                   )) {      // Language ID = 0x0409 = English (United States)
            ser_no_string_desc[0] = 26U;    // Total size of String Descriptor
            ser_no_string_desc[1] = USB_STRING_DESCRIPTOR_TYPE;   // String Descriptor Type
            uint32_t *pUID = (uint32_t *) UID_BASE;
            sprintf( &ser_no_string_desc[2], "TB2.%08x%08x%08x", pUID[0], pUID[1], pUID[2] );
            //memcpy(&ser_no_string_desc[2], L"0002B0000001", 24);  // Serial Number String value
            *buf = (uint8_t *) &ser_no_string_desc[0];      // Return pointer to prepared String Descriptor
            dbgEvtS( TB_usbDescr, ser_no_string_desc );
            logUEvt( &ser_no_string_desc[2] );
            if (setup_packet->wLength >= 26) {
              *len = 26U;                   // Number of bytes of whole String Descriptor
            } else {
              *len = setup_packet->wLength; // Requested number of bytes of String Descriptor
            }
            handle_request = true;          // This request is handled
            return usbdRequestOK;           // Return status that custom handling for this request is used
          }
          break;
        case USB_REQUEST_CLASS:
          break;
        case USB_REQUEST_VENDOR:
          break;
        case USB_REQUEST_RESERVED:
          break;
      }
      return usbdRequestNotProcessed;
      */
}


void USBD_Device0_Endpoint0_SetupPacketProcessed( const USB_SETUP_PACKET *setup_packet ) { // Callback when Device 0 received SETUP PACKET was processed by USB library
    dbgEvt( TB_usbSuProc, ((char *) setup_packet )[0], setup_packet->bRequest, setup_packet->wValue,
            setup_packet->wIndex );
}

usbdRequestStatus USBD_Device0_Endpoint0_OutDataReceived( uint32_t len ) {   // Callback on OUT DATA for USBD0 Control EP0
    //  => usbdRequestNotProcessed, usbdRequestOK, usbdRequestStall, or usbdRequestNAK
    dbgEvt( TB_usbEpOut, len, 0, 0, 0 );
    return usbdRequestNotProcessed;
}

usbdRequestStatus USBD_Device0_Endpoint0_InDataSent( uint32_t len ) {    // Callback on IN DATA for USBD0 Control EP0
    //   usbdRequestNotProcessed, usbdRequestOK, usbdRequestStall, or usbdRequestNAK
    dbgEvt( TB_usbEpIn, len, 0, 0, 0 );
    return usbdRequestNotProcessed;
}

//
//
//************************   USB_User_MSC callbacks to hook into middleware (override weak definitions from USBD driver)
void USBD_MSC0_Initialize( void ) {                   // callback from USBD_Initialize to initialize all Logical Units of the USB MSC class instance.
    //  EventRecorderDisable( EventRecordAll,   EvtFsCore_No,   EvtUsbdEnd_No );    //FS & USB off
    dbgEvt( TB_usbM0Init, 0, 0, 0, 0 );
    for (int i = 0; i < nLUNs; i++)
        initLUN( i );
    usbProvidingMassStorage = true;
    LedManager::ledFg( tbConfig.fgUSB_MSC );      // connected
}

void USBD_MSC0_Uninitialize( void ) {                 // callback from USBD_Uninitialize to de-init all Logical Units of the USB MSC class instance.
    dbgEvt( TB_usbM0Uninit, 0, 0, 0, 0 );
    for (int i = 0; i < nLUNs; i++)
        uninitLUN( i );
}

bool getCacheInfoLUN( uint8_t lun, uint32_t *buffer, uint32_t *size ) {    // set buffer & size to FileSystem cache from fs_ioc_get_cache
    if ( !LUN[lun].media_ok ) return false;

    fsIOC_Cache cache_info;
    int         stat = fs_ioc_get_cache( LUN[lun].drv_id, &cache_info );  // Get cache settings of File System
    dbgEvt( TB_usbCachInfo, lun, stat, (int) cache_info.buffer, cache_info.size );
    if ( stat != fsOK ) return false;

    // Use File Systems cache for MSC
    *buffer = (uint32_t) cache_info.buffer;  // Cache buffer from File System
    *size   = cache_info.size;              // Cache size
    return true;
}

bool USBD_MSC0_GetCacheInfo( uint32_t *buffer, uint32_t *size ) {    // Get cache info: sets cache buffer address & size  => true if success
    return getCacheInfoLUN( 0, buffer, size );    // use LUN0 for all ?!
}

uint8_t USBD_MSC0_GetMaxLUN( void ) {                   // return maximum Index of logical units
    dbgEvt( TB_usbM0getMaxL, nLUNs - 1, 0, 0, 0 );
    return nLUNs - 1;                     // max index of configured file drives
}

bool USBD_MSC0_LUN_GetMediaCapacity( uint8_t lun, uint32_t *block_count, uint32_t *block_size ) { // set #blocks & blocksize for logical unit => true if success
    fsMediaInfo media_info;
    if ( !LUN[lun].media_ok ) return false;
    int stat = fs_ioc_read_info( LUN[lun].drv_id, &media_info );  // Read media information of actual media
    dbgEvt( TB_usbM0getCap, lun, stat, media_info.block_cnt, media_info.read_blen );
    if ( stat != fsOK ) return false;

    *block_count = media_info.block_cnt;    // Total number of blocks on media
    *block_size  = media_info.read_blen;    // Block size of blocks on media
    return true;
}

bool USBD_MSC0_LUN_Read( uint8_t lun, uint32_t lba, uint32_t cnt, uint8_t *buf ) {    // read 'cnt' blocks starting at 'lba' into 'buf'  => true if success
    if ( !LUN[lun].media_ok ) return false;

    int stat = fs_ioc_read_sector( LUN[lun].drv_id, lba, buf, cnt );   // Read data directly from media
    dbgEvt( TB_usbM0read, lun, stat, lba, cnt );
    return ( stat == fsOK );
}

bool USBD_MSC0_LUN_Write( uint8_t lun, uint32_t lba, uint32_t cnt, const uint8_t *buf ) { // write 'cnt' blocks starting at 'lba' from 'buf'  => true if success
    if ( !LUN[lun].media_ok ) return false;

    int stat = fs_ioc_write_sector( LUN[lun].drv_id, lba, buf, cnt );   // Write data directly to media
    dbgEvt( TB_usbM0write, lun, stat, lba, cnt );
    return ( stat == fsOK );
}

uint32_t USBD_MSC0_LUN_CheckMedia( uint8_t lun ) {       // => status for logical unit:  bit 0: 1 if media present, bit 1: 1 if write-protected
    if ( lun >= nLUNs || !LUN[lun].media_ok )
        return 0;     // drive is not ready

    uint32_t fsstatus;
    int      stat = fs_ioc_device_ctrl( LUN[lun].drv_id, fsDevCtrlCodeCheckMedia, &fsstatus );
    dbgEvt( TB_usbM0chkMed, lun, stat, fsstatus, 0 );

    if ( stat == fsOK && ( fsstatus & FS_MEDIA_PROTECTED ) != 0 )
        return USBD_MSC_MEDIA_PROTECTED | USBD_MSC_MEDIA_READY;

    return USBD_MSC_MEDIA_READY;
    /*
           uint32_t fsstatus;
    static uint8_t  media_ready_ex = 0U;    // Previous media ready state
           uint8_t  own;

      uint8_t  media_state = USBD_MSC_MEDIA_READY;


      int stat =  fs_ioc_device_ctrl( LUN[ lun ].drv_id, fsDevCtrlCodeCheckMedia, &fsstatus );

      if ( stat == fsOK && (fsstatus & FS_MEDIA_PROTECTED) != 0 ) {
         media_state |=  USBD_MSC_MEDIA_PROTECTED;
      }

      // Store current owner so no new request can interfere
      own = usbd_msc0_media_own;

      // De-initialize media according to previous owner
      if (own & MEDIA_OWN_CHG) {                    // If owner change requested
        if (own & MEDIA_OWN_USB) {                  // If new requested owner is USB (previous owner was File System)
          funmount (MEDIA_DRIVE);                   // De-initialize media and dismount Drive
        } else {                                    // If new requested owner is File System (previous owner was USB)
          fs_ioc_unlock (drv_id);                   // Un-lock media
        }
      }

      // Initialize media according to current owner
      if ((own & MEDIA_OWN_CHG)        ||           // If owner change requested or
          (media_state ^ media_ready_ex)) {         // if media ready state has changed (disconnect(SD remove)/connect(SD insert))
        if (media_state & USBD_MSC_MEDIA_READY) {   // If media is ready
          if (own & MEDIA_OWN_USB){                 // If current owner is USB
            media_ok     = false;                   // Invalidate current media status (not initialized = not ok)
            param_status = 0U;                      // Parameter for function call is 0
            if (fs_ioc_device_ctrl (drv_id, fsDevCtrlCodeControlMedia, &param_status) == fsOK) {
                                                    // Initialization of media has succeeded
              if (fs_ioc_lock (drv_id) == 0) {      // If lock media for USB usage has succeeded
                media_ok = true;                    // Media was initialized and is ok
              }
            }
          } else {                                  // If current owner is File System
            if (fmount (MEDIA_DRIVE) == fsOK) {     // Initialize media and Mount Drive for File System usage
              media_ok = true;                      // Media was initialized and is ok
            }
          }
        }
        if (own & MEDIA_OWN_CHG) {
          usbd_msc0_media_own &= ~MEDIA_OWN_CHG;    // Clear request to change media owner if it was handled
        }
        media_ready_ex = media_state & USBD_MSC_MEDIA_READY;
      }

      // If media is not ok or owned by File System return that it is not ready for USB
      if ((!media_ok) || (!(usbd_msc0_media_own & MEDIA_OWN_USB))) {
        return 0U;
      }

      return media_state;
    */
}

//  end  USBD_User_Device_MSC_LUN.c
