/* GStreamer
 * Copyright (C) 2021 Allied Vision Technologies GmbH
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2.0 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_vimbasrc_H_
#define _GST_vimbasrc_H_

#include "pixelformats.h"

#include <gst/base/gstpushsrc.h>
#include <glib.h>

#include <VimbaC/Include/VimbaC.h>
#include <VimbaC/Include/VmbCommonTypes.h>

G_BEGIN_DECLS

#define GST_TYPE_vimbasrc (gst_vimbasrc_get_type())
#define GST_vimbasrc(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_vimbasrc, GstVimbaSrc))
#define GST_vimbasrc_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_vimbasrc, GstVimbaSrcClass))
#define GST_IS_vimbasrc(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_vimbasrc))
#define GST_IS_vimbasrc_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_vimbasrc))

/* Allowed values for "Auto" camera Features */
typedef enum
{
    GST_VIMBASRC_AUTOFEATURE_OFF,
    GST_VIMBASRC_AUTOFEATURE_ONCE,
    GST_VIMBASRC_AUTOFEATURE_CONTINUOUS
} GstVimbasrcAutoFeatureValue;

typedef struct _GstVimbaSrc GstVimbaSrc;
typedef struct _GstVimbaSrcClass GstVimbaSrcClass;

#define NUM_VIMBA_FRAMES 3

// global queue in which filled Vimba frames are placed in the vimba_frame_callback
// (has to be global as no context can be passed to VmbFrameCallback functions)
GAsyncQueue *g_filled_frame_queue;

struct _GstVimbaSrc
{
    GstPushSrc base_vimbasrc;

    struct
    {
        const gchar *id;
        VmbHandle_t handle;
        VmbUint32_t supported_formats_count;
        // TODO: This overallocates since no camera will actually support all possible format
        // matches. Allocate and fill at runtime?
        const VimbaGstFormatMatch_t *supported_formats[NUM_FORMAT_MATCHES];
    } camera;

    VmbFrame_t frame_buffers[NUM_VIMBA_FRAMES];
};

struct _GstVimbaSrcClass
{
    GstPushSrcClass base_vimbasrc_class;
};

GType gst_vimbasrc_get_type(void);

G_END_DECLS

VmbError_t alloc_and_announce_buffers(GstVimbaSrc *vimbasrc);
void revoke_and_free_buffers(GstVimbaSrc *vimbasrc);
VmbError_t start_image_acquisition(GstVimbaSrc *vimbasrc);
VmbError_t stop_image_acquisition(GstVimbaSrc *vimbasrc);
void VMB_CALL vimba_frame_callback(const VmbHandle_t cameraHandle, VmbFrame_t *pFrame);
void query_supported_pixel_formats(GstVimbaSrc *vimbasrc);

#endif
