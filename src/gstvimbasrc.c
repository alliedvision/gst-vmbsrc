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
/**
 * SECTION:element-gstvimbasrc
 *
 * The vimbasrc element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! vimbasrc ! FIXME ! fakesink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#include "gstvimbasrc.h"
#include "vimba_helpers.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video-info.h>
#include <glib.h>

#include <VimbaC/Include/VimbaC.h>

GST_DEBUG_CATEGORY_STATIC(gst_vimbasrc_debug_category);
#define GST_CAT_DEFAULT gst_vimbasrc_debug_category

/* prototypes */

static void gst_vimbasrc_set_property(GObject *object,
                                      guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_vimbasrc_get_property(GObject *object,
                                      guint property_id, GValue *value, GParamSpec *pspec);
static void gst_vimbasrc_dispose(GObject *object);
static void gst_vimbasrc_finalize(GObject *object);

static GstCaps *gst_vimbasrc_get_caps(GstBaseSrc *src, GstCaps *filter);
static gboolean gst_vimbasrc_set_caps(GstBaseSrc *src, GstCaps *caps);
static gboolean gst_vimbasrc_start(GstBaseSrc *src);
static gboolean gst_vimbasrc_stop(GstBaseSrc *src);

static GstFlowReturn gst_vimbasrc_create(GstPushSrc *src, GstBuffer **buf);

enum
{
    PROP_0,
    PROP_CAMERA_ID
};

/* pad templates */
// TODO: Add Bayer formats to template
// TODO: What other formats are needed in the template?
static GstStaticPadTemplate gst_vimbasrc_src_template =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)));

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstVimbaSrc, gst_vimbasrc, GST_TYPE_PUSH_SRC,
                        GST_DEBUG_CATEGORY_INIT(gst_vimbasrc_debug_category, "vimbasrc", 0,
                                                "debug category for vimbasrc element"));

static void
gst_vimbasrc_class_init(GstVimbaSrcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
      base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
                                              &gst_vimbasrc_src_template);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          "Vimba GStreamer source", "Generic", DESCRIPTION,
                                          "Allied Vision Technologies GmbH");

    gobject_class->set_property = gst_vimbasrc_set_property;
    gobject_class->get_property = gst_vimbasrc_get_property;
    gobject_class->dispose = gst_vimbasrc_dispose;
    gobject_class->finalize = gst_vimbasrc_finalize;
    base_src_class->get_caps = GST_DEBUG_FUNCPTR(gst_vimbasrc_get_caps);
    base_src_class->set_caps = GST_DEBUG_FUNCPTR(gst_vimbasrc_set_caps);
    base_src_class->start = GST_DEBUG_FUNCPTR(gst_vimbasrc_start);
    base_src_class->stop = GST_DEBUG_FUNCPTR(gst_vimbasrc_stop);
    push_src_class->create = GST_DEBUG_FUNCPTR(gst_vimbasrc_create);

    // Install properties
    g_object_class_install_property(
        gobject_class,
        PROP_CAMERA_ID,
        g_param_spec_string(
            "camera",
            "Camera",
            "ID of the camera images should be recorded from",
            "",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_vimbasrc_init(GstVimbaSrc *vimbasrc)
{
    GST_DEBUG_OBJECT(vimbasrc, "init");
    /* TODO:
        - Start Vimba System here (VmbStartup)
        - Scan for cameras (VmbCamerasList)
    */
    // Start the Vimba API
    VmbError_t result = VmbStartup();
    GST_DEBUG_OBJECT(vimbasrc, "VmbStartup returned: %s", ErrorCodeToMessage(result));
    if (result != VmbErrorSuccess)
    {
        GST_ERROR_OBJECT(vimbasrc, "Vimba initialization failed");
    }

    // Log the used VimbaC version
    VmbVersionInfo_t version_info;
    result = VmbVersionQuery(&version_info, sizeof(version_info));
    if (result == VmbErrorSuccess)
    {
        GST_INFO_OBJECT(vimbasrc, "Running with VimbaC Version %u.%u.%u", version_info.major, version_info.minor, version_info.patch);
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc, "VmbVersionQuery failed with Reason: %s", ErrorCodeToMessage(result));
    }

    // Mark this element as a live source (disable preroll)
    gst_base_src_set_live(GST_BASE_SRC(vimbasrc), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(vimbasrc), GST_FORMAT_TIME);
    gst_base_src_set_do_timestamp(GST_BASE_SRC(vimbasrc), TRUE);
}

void gst_vimbasrc_set_property(GObject *object, guint property_id,
                               const GValue *value, GParamSpec *pspec)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(object);

    GST_DEBUG_OBJECT(vimbasrc, "set_property");

    switch (property_id)
    {
    case PROP_CAMERA_ID:
        vimbasrc->camera.id = g_value_get_string(value);
        VmbError_t result = VmbCameraOpen(vimbasrc->camera.id, VmbAccessModeFull, &vimbasrc->camera.handle);
        if (result == VmbErrorSuccess)
        {
            GST_INFO_OBJECT(vimbasrc, "Successfully opened camera %s", vimbasrc->camera.id);
        }
        else
        {
            GST_ERROR_OBJECT(vimbasrc, "Could not open camera %s. Got error code: %s", vimbasrc->camera.id, ErrorCodeToMessage(result));
            // TODO: List available cameras in this case?
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_vimbasrc_get_property(GObject *object, guint property_id,
                               GValue *value, GParamSpec *pspec)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(object);

    GST_DEBUG_OBJECT(vimbasrc, "get_property");

    switch (property_id)
    {
    case PROP_CAMERA_ID:
        g_value_set_string(value, vimbasrc->camera.id);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_vimbasrc_dispose(GObject *object)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(object);

    GST_DEBUG_OBJECT(vimbasrc, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_vimbasrc_parent_class)->dispose(object);
}

void gst_vimbasrc_finalize(GObject *object)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(object);

    GST_DEBUG_OBJECT(vimbasrc, "finalize");

    VmbError_t result = VmbCameraClose(vimbasrc->camera.handle);
    if (result == VmbErrorSuccess)
    {
        GST_INFO_OBJECT(vimbasrc, "Closed camera %s", vimbasrc->camera.id);
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc, "Closing camera %s failed. Got error code: %s", vimbasrc->camera.id, ErrorCodeToMessage(result));
    }

    VmbShutdown();
    GST_DEBUG_OBJECT(vimbasrc, "Vimba API was shut down");

    G_OBJECT_CLASS(gst_vimbasrc_parent_class)->finalize(object);
}

/* get caps from subclass */
static GstCaps *
gst_vimbasrc_get_caps(GstBaseSrc *src, GstCaps *filter)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "get_caps");

    GstCaps *caps;
    caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    caps = gst_caps_make_writable(caps);

    // TODO: Query the capabilities from the camera and return sensible values
    GstStructure *raw_caps = gst_caps_get_structure(caps, 0);
    gst_structure_set(raw_caps,
        "width", GST_TYPE_INT_RANGE, 1, 2592,
        "height", GST_TYPE_INT_RANGE, 1, 1944,
        "framerate", GST_TYPE_FRACTION, 10, 1,
        NULL
    );
    // TODO: Query supported pixel formats from camera and map them to GStreamer formats
    GValue pixel_format = G_VALUE_INIT;
    g_value_init(&pixel_format, G_TYPE_STRING);
    g_value_set_static_string(&pixel_format, "GRAY8"); // GStreamer GRAY8 corresponds to Mono8 in Vimba formats
    gst_structure_set_value(raw_caps, "format", &pixel_format);

    GST_DEBUG_OBJECT(vimbasrc, "returning caps: %s", gst_caps_to_string(caps));

    return caps;
}

/* notify the subclass of new caps */
static gboolean
gst_vimbasrc_set_caps(GstBaseSrc *src, GstCaps *caps)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "set_caps");

    GST_DEBUG_OBJECT(vimbasrc, "caps requested to be set: %s", gst_caps_to_string(caps));

    // TODO: Actually set the capabilites

    return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_vimbasrc_start(GstBaseSrc *src)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "start");

    /* TODO:
        - Clarify how Hardware triggering influences the setup required here
        - Check if some state variables (is_acquiring, etc.) are helpful and should be added
    */

    // Prepare queue for filled frames from which vimbasrc_create can take them
    g_filled_frame_queue = g_async_queue_new();

    // Determine required buffer size and allocate memory
    VmbInt64_t payload_size;
    VmbError_t result = VmbFeatureIntGet(vimbasrc->camera.handle, "PayloadSize", &payload_size);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Got PayloadSize of: %llu", payload_size);
        for (int i = 0; i < NUM_VIMBA_FRAMES; i++)
        {
            vimbasrc->frame_buffers[i].buffer = (unsigned char *)malloc((VmbUint32_t)payload_size);
            if (NULL == vimbasrc->frame_buffers[i].buffer)
            {
                result = VmbErrorResources;
                break;
            }
            vimbasrc->frame_buffers[i].bufferSize = (VmbUint32_t)payload_size;

            // Announce Frame
            result = VmbFrameAnnounce(vimbasrc->camera.handle, &vimbasrc->frame_buffers[i], (VmbUint32_t)sizeof(VmbFrame_t));
            if (result != VmbErrorSuccess)
            {
                free(vimbasrc->frame_buffers[i].buffer);
                memset(&vimbasrc->frame_buffers[i], 0, sizeof(VmbFrame_t));
                break;
            }
        }

        if (result == VmbErrorSuccess)
        {
            // Start Capture Engine
            result = VmbCaptureStart(vimbasrc->camera.handle);
            if (result == VmbErrorSuccess)
            {
                // g_bStreaming = VmbBoolTrue;
                for (int i = 0; i < NUM_VIMBA_FRAMES; i++)
                {
                    // Queue Frame
                    result = VmbCaptureFrameQueue(vimbasrc->camera.handle, &vimbasrc->frame_buffers[i], &vimba_frame_callback);
                    if (VmbErrorSuccess != result)
                    {
                        break;
                    }
                }

                if (VmbErrorSuccess == result)
                {
                    // Start Acquisition
                    result = VmbFeatureCommandRun(vimbasrc->camera.handle, "AcquisitionStart");
                }
            }
        }
    }

    // Is this necessary?
    gst_base_src_start_complete(src, GST_FLOW_OK);

    // TODO: Is this enough error handling?
    return result == VmbErrorSuccess ? TRUE : FALSE;
}

static gboolean
gst_vimbasrc_stop(GstBaseSrc *src)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "stop");

    // Stop Acquisition
    VmbFeatureCommandRun(vimbasrc->camera.handle, "AcquisitionStop");

    // Stop Capture Engine
    VmbCaptureEnd(vimbasrc->camera.handle);

    // Flush the capture queue
    VmbCaptureQueueFlush(vimbasrc->camera.handle);

    // TODO: Do we need to ensure that revoking is not interrupted by a dangling frame callback?
    // AquireApiLock();?
    for (int i = 0; i < NUM_VIMBA_FRAMES; i++)
    {
        if (NULL != vimbasrc->frame_buffers[i].buffer)
        {
            VmbFrameRevoke(vimbasrc->camera.handle, &vimbasrc->frame_buffers[i]);
            free(vimbasrc->frame_buffers[i].buffer);
            memset(&vimbasrc->frame_buffers[i], 0, sizeof(VmbFrame_t));
        }
    }

    // Unref the filled frame queue so it is deleted properly
    g_async_queue_unref(g_filled_frame_queue);

    return TRUE;
}

/* ask the subclass to create a buffer */
static GstFlowReturn
gst_vimbasrc_create(GstPushSrc *src, GstBuffer **buf)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "create");

    // Wait until we can get a filled frame (added to queue in vimba_frame_callback)
    VmbFrame_t *frame = g_async_queue_pop(g_filled_frame_queue);

    // Prepare output buffer that will be filled with frame data
    GstBuffer *buffer = gst_buffer_new_and_alloc(frame->bufferSize);

    // copy over frame data into the GStreamer buffer
    // TODO: Investigate if we can work without copying to improve performance?
    // TODO: Add handling of incomplete frames here. This assumes that we got nice and working frames
    gst_buffer_fill(
        buffer,
        0,
        frame->buffer,
        frame->bufferSize);

    // requeue frame after we copied the image data for Vimba to use again
    VmbCaptureFrameQueue(vimbasrc->camera.handle, frame, &vimba_frame_callback);

    // Set filled GstBuffer as output to pass down the pipeline
    *buf = buffer;

    return GST_FLOW_OK;
}

static gboolean
plugin_init(GstPlugin *plugin)
{

    /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
    return gst_element_register(plugin, "vimbasrc", GST_RANK_NONE,
                                GST_TYPE_vimbasrc);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  vimbasrc,
                  DESCRIPTION,
                  plugin_init,
                  VERSION,
                  "LGPL",
                  PACKAGE,
                  HOMEPAGE_URL)

void VMB_CALL vimba_frame_callback(const VmbHandle_t camera_handle, VmbFrame_t *frame)
{
    GST_DEBUG("Got Frame");
    g_async_queue_push(g_filled_frame_queue, frame);

    // requeueing the frame is done after it was consumed in vimbasrc_create
}
