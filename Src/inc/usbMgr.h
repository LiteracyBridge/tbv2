#ifndef USB_MGR_H
#define USB_MGR_H

#include	"tbook.h"

extern void		initUSBManager( void );
extern bool		isMassStorageEnabled( void );
extern bool		disableMassStorage( void );

#endif /* USB_MGR_H */
