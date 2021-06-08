#include "main.h"
#include "tbook.h"

/// \brief Callback to retrieve a time to set as a file's timestamp.
/// \param[out] time                     fsTime structure to be filled in. 
/// \param[out] buf                      pointer to string buffer.
/// \return     execution status \ref fsStatus
///               - fsOK               = Operation successful.
///               - fsError            = Failed to get the current time.
fsStatus fs_get_time (fsTime *time) {
  getRTC(time);
  return fsOK;
}
