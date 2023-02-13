#include "vimbax_helpers.h"

#include <VmbC/VmbC.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

//
// Translates VimbaX error codes to readable error messages
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
        return "Invalid handle.";
    case VmbErrorDeviceNotOpen:
        return "Device not open.";
    case VmbErrorInvalidAccess:
        return "Invalid access.";
    case VmbErrorBadParameter:
        return "Bad parameter.";
    case VmbErrorStructSize:
        return "Wrong DLL version.";
    case VmbErrorMoreData:
        return "More data is available.";
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
        return "No TL loaded.";
    case VmbErrorNotImplemented:
        return "Not implemented.";
    case VmbErrorNotSupported:
        return "Not supported.";
    case VmbErrorIncomplete:
        return "Operation is not complete.";
    case VmbErrorIO:
        return "IO error.";
    case VmbErrorValidValueSetNotPresent:
        return "No valid value set available.";
    case VmbErrorGenTLUnspecified:
        return "Unspecified GenTL runtime error.";
    case VmbErrorUnspecified:
        return "Unspecified runtime error.";
    case VmbErrorBusy:
        return "The responsible module/entity is busy executing actions.";
    case VmbErrorNoData:
        return "The function has no data to work on.";
    case VmbErrorParsingChunkData:
        return "An error occurred parsing a buffer containing chunk data.";
    case VmbErrorInUse:
        return "Already in use.";
    case VmbErrorUnknown:
        return "Unknown error condition.";
    case VmbErrorXml:
        return "Error parsing xml.";
    case VmbErrorNotAvailable:
        return "Something is not available.";
    case VmbErrorNotInitialized:
        return "Something is not initialized.";
    case VmbErrorInvalidAddress:
        return "The given address is out of range or invalid for internal reasons.";
    case VmbErrorAlready:
        return "Something has already been done.";
    case VmbErrorNoChunkData:
        return "A frame expected to contain chunk data does not contain chunk data.";
    case VmbErrorUserCallbackException:
        return "A callback provided by the user threw an exception.";
    case VmbErrorFeaturesUnavailable:
        return "Feature unavailable for a module.";
    case VmbErrorTLNotFound:
        return "A required transport layer could not be found or loaded.";
    case VmbErrorAmbiguous:
        return "Entity cannot be uniquely identified based on the information provided.";
    case VmbErrorRetriesExceeded:
        return "Allowed retries exceeded without successfully completing the operation.";
    default:
        return eError >= VmbErrorCustom ? "User defined error" : "Unknown";
    }
}

//
// Round a value to the nearest valid value in the given interval with the given increment
//
// Parameters:
//  [in]    value       The value to find the nearest valid value for
//  [in]    min         Minimum valid value (inclusive)
//  [in]    max         Maximum valid value (inclusive)
//  [in]    increment   Increment/Spacing of the valid values in the interval
//
// Returns:
//  The nearest valid value inside the given interval. If value was already valid, value is returned.
//
VmbInt64_t RoundToNearestValidValue(VmbInt64_t value, VmbInt64_t min, VmbInt64_t max, VmbInt64_t increment)
{
    VmbInt64_t steps = (value - min) / increment;
    VmbInt64_t error = value - (steps * increment);
    if ((error * 2) > increment)
    {
        steps += 1;
    }
    return MAX(MIN((steps * increment) + min, max), min);
}
