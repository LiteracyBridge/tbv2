// TBookV2  tb_enum.h
//  Oct2021
#ifndef TB_ENUM_H
#define TB_ENUM_H

#include "tb_enum_list.h"

#define X(v) v,
enum CSM_EVENT {
    CSM_EVENT_LIST
    NUM_CSM_EVENTS
};
#undef X

#define X(v) v,
enum CSM_ACTION {
    CSM_ACTION_LIST
    NUM_CSM_ACTIONS
};
#undef X

#define X(name,path) p ## name,
enum TB_FILENAMES {
    FILENAME_LIST
    NUM_FILENAMES
};
#undef X

#endif  /* tb_enum.h */
