/system/ directory for TalkingBook

device_ID.txt
   -- written with Unique Processor ID for this device 
   
tbook_names.txt
   -- associations between Unique Processor ID and a more meaningful names
   -- lines of the form  xxxx.xxxx.x.Qxxxxxx Name_for_talking_book
   -- where 'x' are hexadecimal characters in the Unique Processor ID of a TalkingBook

bootcnt.txt
   "system/bootcnt.txt" contains number of time system has rebooted for current log file
   if "system/bootcnt.txt" doesn't exist:
      at next reboot the TalkingBook will start with new event Log & bootcnt.txt holds " 1"
      and if "system/EraseNorLog.txt" DOES exist:
         the entire NorLog memory is erased
         /LOG/tbLog_0.txt  is the name of the first log file
      otherwise, /Log/tbLog_N.txt (for new log N)  
   /LOG/tbLog_N.txt  is written with the internal version of the log before entering USB mode

setRTC.txt, lastRTC.txt, dontSetRTC.txt
  at TBook boot:
    if /system/setRTC.txt file exists:
        the TBook RTC will be set from the creation date/time of setRTC.txt
        and setRTC.txt is renamed to dontSetRTC.txt
    if the TBook RTC has been reset (i.e. from a battery change while not on USB power)
        the RTC will be reset from the creation date/time of lastRTC.txt
  at TBook normal power down:
    /system/lastRTC.txt is rewritten as "---", updating its date/time from current RTC setting

csm_data.txt
   CSM definition (translated from control_def.txt by csmCompile)
      defines the Control State Machine for the Talking Book 
      first line describes the version of this file
      e.g. // TB CSM 2021-05-12: V8 with QC test
      ( see Amplio document: "Talking Book V2" )
control_def_vXX.txt
   Source file for csm_data.txt  (not referenced by TBook firmware)
   
dontEraseNorLog.txt
   control file for erasing entire NOR flash (details within)

QC_PASS.txt, QC_FAIL.txt, qcAudio.wav
   QC_PASS.txt contains results of initial QcTest 
     contents of "QC_OK" if successful (or "QC_short_OK" if abbreviated test)
   QC_FAIL.txt contains reason for failure
   qcAudio.wav  is the short audio recorded during the QcTest   

