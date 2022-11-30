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

#ifndef _GST_vimbaxsrc_H_
#define _GST_vimbaxsrc_H_

#include "pixelformats.h"

#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include <glib.h>

#include <VmbC/VmbC.h>
#include <VmbC/VmbCommonTypes.h>

#include <stdbool.h>

G_BEGIN_DECLS

#define GST_TYPE_vimbaxsrc (gst_vimbaxsrc_get_type())
#define GST_vimbaxsrc(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_vimbaxsrc, GstVimbaSrc))
#define GST_vimbaxsrc_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_vimbaxsrc, GstVimbaSrcClass))
#define GST_IS_vimbaxsrc(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_vimbaxsrc))
#define GST_IS_vimbaxsrc_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_vimbaxsrc))

/* Allowed values for "Auto" camera Features */
typedef enum
{
    GST_VIMBAXSRC_AUTOFEATURE_OFF,
    GST_VIMBAXSRC_AUTOFEATURE_ONCE,
    GST_VIMBAXSRC_AUTOFEATURE_CONTINUOUS
} GstVimbasrcAutoFeatureValue;

// Possible values for TriggerSelector feature
typedef enum
{
    GST_VIMBAXSRC_TRIGGERSELECTOR_UNCHANGED,
    GST_VIMBAXSRC_TRIGGERSELECTOR_ACQUISITION_START,
    GST_VIMBAXSRC_TRIGGERSELECTOR_ACQUISITION_END,
    GST_VIMBAXSRC_TRIGGERSELECTOR_ACQUISITION_ACTIVE,
    GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_START,
    GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_END,
    GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_ACTIVE,
    GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_BURST_START,
    GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_BURST_END,
    GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_BURST_ACTIVE,
    GST_VIMBAXSRC_TRIGGERSELECTOR_LINE_START,
    GST_VIMBAXSRC_TRIGGERSELECTOR_EXPOSURE_START,
    GST_VIMBAXSRC_TRIGGERSELECTOR_EXPOSURE_END,
    GST_VIMBAXSRC_TRIGGERSELECTOR_EXPOSURE_ACTIVE
} GstVimbasrcTriggerSelectorValue;

// Possible values for TriggerMode feature
typedef enum
{
    GST_VIMBAXSRC_TRIGGERMODE_UNCHANGED,
    GST_VIMBAXSRC_TRIGGERMODE_OFF,
    GST_VIMBAXSRC_TRIGGERMODE_ON
} GstVimbasrcTriggerModeValue;

// Possible values for the TriggerSource feature
typedef enum
{
    GST_VIMBAXSRC_TRIGGERSOURCE_UNCHANGED,
    GST_VIMBAXSRC_TRIGGERSOURCE_SOFTWARE,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINE0,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINE1,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINE2,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINE3,
    GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT0,
    GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT1,
    GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT2,
    GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT3,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER0_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER1_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER2_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER3_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER0_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER1_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER2_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER3_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER0_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER1_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER2_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER3_START,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER0_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER1_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER2_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_TIMER3_END,
    GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER0,
    GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER1,
    GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER2,
    GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER3,
    GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK0,
    GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK1,
    GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK2,
    GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK3,
    GST_VIMBAXSRC_TRIGGERSOURCE_ACTION0,
    GST_VIMBAXSRC_TRIGGERSOURCE_ACTION1,
    GST_VIMBAXSRC_TRIGGERSOURCE_ACTION2,
    GST_VIMBAXSRC_TRIGGERSOURCE_ACTION3,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER0,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER1,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER2,
    GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER3
} GstVimbasrcTriggerSourceValue;

// Possible values for TriggerActivation feature
typedef enum
{
    GST_VIMBAXSRC_TRIGGERACTIVATION_UNCHANGED,
    GST_VIMBAXSRC_TRIGGERACTIVATION_RISING_EDGE,
    GST_VIMBAXSRC_TRIGGERACTIVATION_FALLING_EDGE,
    GST_VIMBAXSRC_TRIGGERACTIVATION_ANY_EDGE,
    GST_VIMBAXSRC_TRIGGERACTIVATION_LEVEL_HIGH,
    GST_VIMBAXSRC_TRIGGERACTIVATION_LEVEL_LOW
} GstVimbasrcTriggerActivationValue;

// Implemented handling approaches for incomplete frames
typedef enum
{
    GST_VIMBAXSRC_INCOMPLETE_FRAME_HANDLING_DROP,
    GST_VIMBAXSRC_INCOMPLETE_FRAME_HANDLING_SUBMIT
} GstVimbasrcIncompleteFrameHandlingValue;

typedef struct _GstVimbaSrc GstVimbaSrc;
typedef struct _GstVimbaSrcClass GstVimbaSrcClass;

#define NUM_VIMBA_FRAMES 3

struct _GstVimbaSrc
{
    GstPushSrc base_vimbaxsrc;

    struct
    {
        char *id;
        VmbHandle_t handle;
        VmbCameraInfo_t info;
        VmbUint32_t supported_formats_count;
        // TODO: This overallocates since no camera will actually support all possible format matches. Allocate and fill
        // at runtime?
        const VimbaGstFormatMatch_t *supported_formats[NUM_FORMAT_MATCHES];
        bool is_connected;
        bool is_acquiring;
    } camera;
    struct
    {
        char *settings_file_path;
        double exposuretime;
        int exposureauto;
        int balancewhiteauto;
        double gain;
        int offsetx;
        int offsety;
        int width;
        int height;
        int triggerselector;
        int triggermode;
        int triggersource;
        int triggeractivation;
        int incomplete_frame_handling;
    } properties;

    VmbFrame_t frame_buffers[NUM_VIMBA_FRAMES];
    // queue in which filled Vimba frames are placed in the vimba_frame_callback (attached to each queued frame at
    // frame->context[0])
    GAsyncQueue *filled_frame_queue;
    GstVideoInfo video_info;
};

struct _GstVimbaSrcClass
{
    GstPushSrcClass base_vimbaxsrc_class;
};

GType gst_vimbaxsrc_get_type(void);

G_END_DECLS

VmbError_t open_camera_connection(GstVimbaSrc *vimbaxsrc);
VmbError_t apply_feature_settings(GstVimbaSrc *vimbaxsrc);
VmbError_t set_roi(GstVimbaSrc *vimbaxsrc);
VmbError_t apply_trigger_settings(GstVimbaSrc *vimbaxsrc);
VmbError_t alloc_and_announce_buffers(GstVimbaSrc *vimbaxsrc);
void revoke_and_free_buffers(GstVimbaSrc *vimbaxsrc);
VmbError_t start_image_acquisition(GstVimbaSrc *vimbaxsrc);
VmbError_t stop_image_acquisition(GstVimbaSrc *vimbaxsrc);
void VMB_CALL vimba_frame_callback(const VmbHandle_t cameraHandle, const VmbHandle_t stream_handle, VmbFrame_t *pFrame);
void map_supported_pixel_formats(GstVimbaSrc *vimbaxsrc);
void log_available_enum_entries(GstVimbaSrc *vimbaxsrc, const char *feat_name);

#endif
