#include "pixelformats.h"
#include <stddef.h>
#include <string.h>

const VimbaXGstFormatMatch_t *gst_format_from_vimbax_format(const char *vimbax_format)
{
    for (unsigned int i = 0; i < NUM_FORMAT_MATCHES; i++)
    {
        if (strcmp(vimbax_format, vimbax_gst_format_matches[i].vimbax_format_name) == 0)
        {
            return &vimbax_gst_format_matches[i];
        }
    }
    return NULL;
}

// TODO: There may be multiple VimbaX format entries for the same gst_format. How to handle this? Currently the first hit
// for the gst_format is returned and the rest ignored.
const VimbaXGstFormatMatch_t *vimbax_format_from_gst_format(const char *gst_format)
{
    for (unsigned int i = 0; i < NUM_FORMAT_MATCHES; i++)
    {
        if (strcmp(gst_format, vimbax_gst_format_matches[i].gst_format_name) == 0)
        {
            return &vimbax_gst_format_matches[i];
        }
    }
    return NULL;
}