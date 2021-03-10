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

#include <stdbool.h>

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

// Possible values for TriggerSelector feature
// TODO: Which of these are really needed?
typedef enum
{
    GST_VIMBASRC_TRIGGERSELECTOR_ACQUISITION_START,
    GST_VIMBASRC_TRIGGERSELECTOR_ACQUISITION_END,
    GST_VIMBASRC_TRIGGERSELECTOR_ACQUISITION_ACTIVE,
    GST_VIMBASRC_TRIGGERSELECTOR_FRAME_START,
    GST_VIMBASRC_TRIGGERSELECTOR_FRAME_END,
    GST_VIMBASRC_TRIGGERSELECTOR_FRAME_ACTIVE,
    GST_VIMBASRC_TRIGGERSELECTOR_FRAME_BURST_START,
    GST_VIMBASRC_TRIGGERSELECTOR_FRAME_BURST_END,
    GST_VIMBASRC_TRIGGERSELECTOR_FRAME_BURST_ACTIVE,
    GST_VIMBASRC_TRIGGERSELECTOR_LINE_START,
    GST_VIMBASRC_TRIGGERSELECTOR_EXPOSURE_START,
    GST_VIMBASRC_TRIGGERSELECTOR_EXPOSURE_END,
    GST_VIMBASRC_TRIGGERSELECTOR_EXPOSURE_ACTIVE,
    GST_VIMBASRC_TRIGGERSELECTOR_MULTI_SLOPE_EXPOSURE_LIMIT1
} GstVimbasrcTriggerSelectorValue;

// Possible values for TriggerMode feature
typedef enum
{
    GST_VIMBASRC_TRIGGERMODE_OFF,
    GST_VIMBASRC_TRIGGERMODE_ON
} GstVimbasrcTriggerModeValue;

// Possible values for the TriggerSource feature
// TODO: which of these are really needed? Current entries taken from SFNC
typedef enum
{
    GST_VIMBASRC_TRIGGERSOURCE_SOFTWARE,         // is this really supported by our plugin? We would need to periodically run the TriggerSoftware command feature
    GST_VIMBASRC_TRIGGERSOURCE_SOFTWARE_SIGNAL0, // and 1, 2, etc. Do we support the needed SoftwareSignalPulse command?
    GST_VIMBASRC_TRIGGERSOURCE_LINE0,            // How many Line enum entries do we need?
    GST_VIMBASRC_TRIGGERSOURCE_LINE1,
    GST_VIMBASRC_TRIGGERSOURCE_LINE2,
    GST_VIMBASRC_TRIGGERSOURCE_LINE3,
    GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT0, // How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT1,
    GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT2,
    GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT3,
    GST_VIMBASRC_TRIGGERSOURCE_COUNTER0_START, // and 1, 2, etc. Actually supported by any camera? How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_COUNTER0_END,   // and 1, 2, etc. Actually supported by any camera? How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_TIMER0_START,   // and 1, 2, etc. Actually supported by any camera? How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_TIMER0_END,     // and 1, 2, etc. Actually supported by any camera? How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_ENCODER0,       // How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_ENCODER1,
    GST_VIMBASRC_TRIGGERSOURCE_ENCODER2,
    GST_VIMBASRC_TRIGGERSOURCE_ENCODER3,
    GST_VIMBASRC_TRIGGERSOURCE_LOGIC_BLOCK0,  // and 1, 2, etc. Actually supported by any camera? How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_ACTION0,       // and 1, 2, etc. Actually supported by any camera? How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_LINK_TRIGGER0, // and 1, 2, etc. Actually supported by any camera? How many do we need?
    GST_VIMBASRC_TRIGGERSOURCE_CC1            // and 1, 2, etc. Actually supported by any camera? How many do we need?
} GstVimbasrcTriggerSourceValue;

// Possible values for TriggerActivation feature
typedef enum
{
    GST_VIMBASRC_TRIGGERACTIVATION_RISING_EDGE,
    GST_VIMBASRC_TRIGGERACTIVATION_FALLING_EDGE,
    GST_VIMBASRC_TRIGGERACTIVATION_ANY_EDGE,
    GST_VIMBASRC_TRIGGERACTIVATION_LEVEL_HIGH,
    GST_VIMBASRC_TRIGGERACTIVATION_LEVEL_LOW
} GstVimbasrcTriggerActivationValue;

// TODO: Do we also need any of the following features?
// - TriggerOverlap
// - TriggerDelay
// - TriggerDivider
// - TriggerMultiplier

typedef struct _GstVimbaSrc GstVimbaSrc;
typedef struct _GstVimbaSrcClass GstVimbaSrcClass;

#define NUM_VIMBA_FRAMES 3

// global queue in which filled Vimba frames are placed in the vimba_frame_callback (has to be global as no context can
// be passed to VmbFrameCallback functions)
GAsyncQueue *g_filled_frame_queue;

struct _GstVimbaSrc
{
    GstPushSrc base_vimbasrc;

    struct
    {
        const gchar *id;
        VmbHandle_t handle;
        VmbUint32_t supported_formats_count;
        // TODO: This overallocates since no camera will actually support all possible format matches. Allocate and fill
        // at runtime?
        const VimbaGstFormatMatch_t *supported_formats[NUM_FORMAT_MATCHES];
        bool is_connected;
        bool is_acquiring;
    } camera;
    struct
    {
        const char *camera_id;
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
    } properties;

    VmbFrame_t frame_buffers[NUM_VIMBA_FRAMES];
};

struct _GstVimbaSrcClass
{
    GstPushSrcClass base_vimbasrc_class;
};

GType gst_vimbasrc_get_type(void);

G_END_DECLS

VmbError_t open_camera_connection(GstVimbaSrc *vimbasrc);
VmbError_t apply_feature_settings(GstVimbaSrc *vimbasrc);
VmbError_t set_roi(GstVimbaSrc *vimbasrc);
VmbError_t apply_trigger_settings(GstVimbaSrc *vimbasrc);
VmbError_t alloc_and_announce_buffers(GstVimbaSrc *vimbasrc);
void revoke_and_free_buffers(GstVimbaSrc *vimbasrc);
VmbError_t start_image_acquisition(GstVimbaSrc *vimbasrc);
VmbError_t stop_image_acquisition(GstVimbaSrc *vimbasrc);
void VMB_CALL vimba_frame_callback(const VmbHandle_t cameraHandle, VmbFrame_t *pFrame);
void map_supported_pixel_formats(GstVimbaSrc *vimbasrc);

#endif
