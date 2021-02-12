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

#include <gst/base/gstpushsrc.h>

#include <VimbaC/Include/VimbaC.h>
#include <VimbaC/Include/VmbCommonTypes.h>

G_BEGIN_DECLS

#define GST_TYPE_vimbasrc (gst_vimbasrc_get_type())
#define GST_vimbasrc(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_vimbasrc, GstVimbaSrc))
#define GST_vimbasrc_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_vimbasrc, GstVimbaSrcClass))
#define GST_IS_vimbasrc(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_vimbasrc))
#define GST_IS_vimbasrc_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_vimbasrc))

typedef struct _GstVimbaSrc GstVimbaSrc;
typedef struct _GstVimbaSrcClass GstVimbaSrcClass;

#define NUM_VIMBA_FRAMES 3

struct _GstVimbaSrc
{
    GstPushSrc base_vimbasrc;

    struct
    {
        const gchar *id;
        VmbHandle_t handle;
    } camera;

    VmbFrame_t frame_buffers[NUM_VIMBA_FRAMES];
};

struct _GstVimbaSrcClass
{
    GstPushSrcClass base_vimbasrc_class;
};

GType gst_vimbasrc_get_type(void);

G_END_DECLS

void VMB_CALL vimba_frame_callback(const VmbHandle_t cameraHandle, VmbFrame_t *pFrame);

#endif
