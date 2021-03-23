#include "vimba_helpers.h"

#include <gst/gstinfo.h>

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
        return "Other error.";
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

// Purpose: Discovers GigE cameras if GigE TL is present.
//          Discovery is switched on only once so that the API can detect all currently connected cameras.
VmbBool_t DiscoverGigECameras(GObject *object)
{
    VmbError_t result = VmbErrorSuccess;
    VmbBool_t isGigE = VmbBoolFalse;

    VmbBool_t ret = VmbBoolFalse;

    // Is Vimba connected to a GigE transport layer?
    result = VmbFeatureBoolGet(gVimbaHandle, "GeVTLIsPresent", &isGigE);
    if (VmbErrorSuccess == result)
    {
        if (VmbBoolTrue == isGigE)
        {
            // Set the waiting duration for discovery packets to return. If not set the default of 150 ms is used.
            result = VmbFeatureIntSet(gVimbaHandle, "GeVDiscoveryAllDuration", 250);
            if (VmbErrorSuccess == result)
            {
                // Send discovery packets to GigE cameras and wait 250 ms until they are answered
                result = VmbFeatureCommandRun(gVimbaHandle, "GeVDiscoveryAllOnce");
                if (VmbErrorSuccess == result)
                {
                    ret = VmbBoolTrue;
                }
                else
                {
                    GST_WARNING_OBJECT(object,
                                       "Could not ping GigE cameras over the network. Reason: %s",
                                       ErrorCodeToMessage(result));
                }
            }
            else
            {
                GST_WARNING_OBJECT(object,
                                   "Could not set the discovery waiting duration. Reason: %s",
                                   ErrorCodeToMessage(result));
            }
        }
        else
        {
            GST_INFO_OBJECT(object, "Vimba is not connected to a GigE transport layer");
        }
    }
    else
    {
        GST_WARNING_OBJECT(object,
                           "Could not query Vimba for the presence of a GigE transport layer. Reason: %s",
                           ErrorCodeToMessage(result));
    }

    return ret;
}
