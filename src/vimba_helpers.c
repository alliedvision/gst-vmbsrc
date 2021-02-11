#include "vimba_helpers.h"
#include <VimbaC/Include/VimbaC.h>

//
// Translates Vimba error codes to readable error messages
//
// Parameters:
//  [in]    eError      The error code to be converted to string
//
// Returns:
//  A descriptive string representation of the error code
//
const char *ErrorCodeToMessage(VmbError_t eError)
{
    switch (eError)
    {
    case VmbErrorSuccess:
        return "Success.";
    case VmbErrorInternalFault:
        return "Unexpected fault in VmbApi or driver.";
    case VmbErrorApiNotStarted:
        return "API not started.";
    case VmbErrorNotFound:
        return "Not found.";
    case VmbErrorBadHandle:
        return "Invalid handle ";
    case VmbErrorDeviceNotOpen:
        return "Device not open.";
    case VmbErrorInvalidAccess:
        return "Invalid access.";
    case VmbErrorBadParameter:
        return "Bad parameter.";
    case VmbErrorStructSize:
        return "Wrong DLL version.";
    case VmbErrorMoreData:
        return "More data returned than memory provided.";
    case VmbErrorWrongType:
        return "Wrong type.";
    case VmbErrorInvalidValue:
        return "Invalid value.";
    case VmbErrorTimeout:
        return "Timeout.";
    case VmbErrorOther:
        return "TL error.";
    case VmbErrorResources:
        return "Resource not available.";
    case VmbErrorInvalidCall:
        return "Invalid call.";
    case VmbErrorNoTL:
        return "TL not loaded.";
    case VmbErrorNotImplemented:
        return "Not implemented.";
    case VmbErrorNotSupported:
        return "Not supported.";
    default:
        return "Unknown";
    }
}
