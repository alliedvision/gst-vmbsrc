#ifndef VIMBA_HELPERS_H_
#define VIMBA_HELPERS_H_

#include <glib-object.h>

#include <VimbaC/Include/VmbCommonTypes.h>

const char *ErrorCodeToMessage(VmbError_t eError);

VmbBool_t DiscoverGigECameras(GObject *object);

#endif // VIMBA_HELPERS_H_
