#include "pixelformats.h"
#include <stddef.h>
#include <string.h>

const VimbaGstFormatMatch_t *gst_format_from_vimba_format(const char *vimba_format)
{
    for (int i = 0; i < NUM_FORMAT_MATCHES; i++)
    {
        if (strcmp(vimba_format, vimba_gst_format_matches[i].vimba_format_name) == 0)
        {
            return &vimba_gst_format_matches[i];
        }
    }
    return NULL;
}

// TODO: There may be multiple vimba format entries for the same gst_format. How to handle this? Currently the first hit
// for the gst_format is returned and the rest ignored.
const VimbaGstFormatMatch_t *vimba_format_from_gst_format(const char *gst_format)
{
    VmbPixelFormat_t detected_format = 0;
    for (int i = 0; i < NUM_FORMAT_MATCHES; i++)
    {
        if (strcmp(gst_format, vimba_gst_format_matches[i].gst_format_name) == 0)
        {
            return &vimba_gst_format_matches[i];
        }
    }
    return NULL;
}