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
#include "helpers.h"
#include "vimba_helpers.h"
#include "pixelformats.h"

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
    PROP_CAMERA_ID,
    PROP_EXPOSUREAUTO,
    PROP_BALANCEWHITEAUTO,
    PROP_GAIN,
    PROP_EXPOSURETIME,
    PROP_OFFSETX,
    PROP_OFFSETY,
    PROP_WIDTH,
    PROP_HEIGHT
};

/* pad templates */
static GstStaticPadTemplate gst_vimbasrc_src_template =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(
                                GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL) ";" GST_BAYER_CAPS_MAKE(GST_BAYER_FORMATS_ALL)));

/* Auto exposure modes */
#define GST_ENUM_EXPOSUREAUTO_MODES (gst_vimbasrc_exposureauto_get_type())
static GType
gst_vimbasrc_exposureauto_get_type(void)
{
    static GType vimbasrc_exposureauto_type = 0;
    static const GEnumValue exposureauto_modes[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the Vimba FeatureEnum */
        {GST_VIMBASRC_AUTOFEATURE_OFF, "Exposure duration is usercontrolled using ExposureTime", "Off"},
        {GST_VIMBASRC_AUTOFEATURE_ONCE, "Exposure duration is adapted once by the device. Once it has converged, it returns to the Offstate", "Once"},
        {GST_VIMBASRC_AUTOFEATURE_CONTINUOUS, "Exposure duration is constantly adapted by the device to maximize the dynamic range", "Continuous"},
        {0, NULL, NULL}};
    if (!vimbasrc_exposureauto_type)
    {
        vimbasrc_exposureauto_type =
            g_enum_register_static("GstVimbasrcExposureAutoModes", exposureauto_modes);
    }
    return vimbasrc_exposureauto_type;
}

/* Auto white balance modes */
#define GST_ENUM_BALANCEWHITEAUTO_MODES (gst_vimbasrc_balancewhiteauto_get_type())
static GType
gst_vimbasrc_balancewhiteauto_get_type(void)
{
    static GType vimbasrc_balancewhiteauto_type = 0;
    static const GEnumValue balancewhiteauto_modes[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the Vimba FeatureEnum */
        {GST_VIMBASRC_AUTOFEATURE_OFF, "White balancing is user controlled using BalanceRatioSelector and BalanceRatio", "Off"},
        {GST_VIMBASRC_AUTOFEATURE_ONCE, "White balancing is automatically adjusted once by the device. Once it has converged, it automatically returns to the Off state", "Once"},
        {GST_VIMBASRC_AUTOFEATURE_CONTINUOUS, "White balancing is constantly adjusted by the device", "Continuous"},
        {0, NULL, NULL}};
    if (!vimbasrc_balancewhiteauto_type)
    {
        vimbasrc_balancewhiteauto_type =
            g_enum_register_static("GstVimbasrcBalanceWhiteAutoModes", balancewhiteauto_modes);
    }
    return vimbasrc_balancewhiteauto_type;
}

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
            "Camera ID",
            "ID of the camera images should be recorded from",
            "",
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_EXPOSUREAUTO,
        g_param_spec_enum(
            "exposureauto",
            "ExposureAuto feature setting",
            "Sets the auto exposure mode. The output of the auto exposure function affects the whole image",
            GST_ENUM_EXPOSUREAUTO_MODES,
            GST_VIMBASRC_AUTOFEATURE_OFF,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_BALANCEWHITEAUTO,
        g_param_spec_enum(
            "balancewhiteauto",
            "BalanceWhiteAuto feature setting",
            "Controls the mode for automatic white balancing between the color channels. The white balancing ratios are automatically adjusted",
            GST_ENUM_BALANCEWHITEAUTO_MODES,
            GST_VIMBASRC_AUTOFEATURE_OFF,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_GAIN,
        g_param_spec_double(
            "gain",
            "Gain feature setting",
            "Controls the selected gain as an absolute physical value. This is an amplification factor applied to the video signal",
            0.,
            G_MAXDOUBLE,
            0.,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_EXPOSURETIME,
        g_param_spec_double(
            "exposuretime",
            "ExposureTime feature setting",
            "Sets the Exposure time (in microseconds) when ExposureMode is Timed and ExposureAuto is Off. This controls the duration where the photosensitive cells are exposed to light",
            0.,
            G_MAXDOUBLE,
            0.,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_OFFSETX,
        g_param_spec_int(
            "offsetx",
            "OffsetX feature setting",
            "Horizontal offset from the origin to the region of interest (in pixels).",
            0,
            G_MAXINT,
            0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_OFFSETY,
        g_param_spec_int(
            "offsety",
            "OffsetY feature setting",
            "Vertical offset from the origin to the region of interest (in pixels).",
            0,
            G_MAXINT,
            0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_WIDTH,
        g_param_spec_int(
            "width",
            "Width feature setting",
            "Width of the image provided by the device (in pixels).",
            0,
            G_MAXINT,
            0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_HEIGHT,
        g_param_spec_int(
            "height",
            "Height feature setting",
            "Height of the image provided by the device (in pixels).",
            0,
            G_MAXINT,
            0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_vimbasrc_init(GstVimbaSrc *vimbasrc)
{
    GST_DEBUG_OBJECT(vimbasrc, "init");
    GST_INFO_OBJECT(vimbasrc, "gst-vimbasrc version %s", VERSION);
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

    if (DiscoverGigECameras((GObject *)vimbasrc) == VmbBoolFalse)
    {
        GST_INFO_OBJECT(vimbasrc, "GigE cameras will be ignored");
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

    VmbError_t result;

    GEnumValue *enum_entry;
    gdouble double_entry;
    gint int_entry;

    switch (property_id)
    {
    case PROP_CAMERA_ID:
        vimbasrc->camera.id = g_value_get_string(value);
        result = VmbCameraOpen(vimbasrc->camera.id, VmbAccessModeFull, &vimbasrc->camera.handle);
        if (result == VmbErrorSuccess)
        {
            GST_INFO_OBJECT(vimbasrc, "Successfully opened camera %s", vimbasrc->camera.id);
            query_supported_pixel_formats(vimbasrc);
        }
        else
        {
            GST_ERROR_OBJECT(vimbasrc, "Could not open camera %s. Got error code: %s", vimbasrc->camera.id, ErrorCodeToMessage(result));
            // TODO: List available cameras in this case?
            // TODO: Can we signal an error to the pipeline to stop immediately?
        }
        break;
    case PROP_EXPOSUREAUTO:
        enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_EXPOSUREAUTO_MODES), g_value_get_enum(value));
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"ExposureAuto\" to %s", enum_entry->value_nick);
        result = VmbFeatureEnumSet(vimbasrc->camera.handle, "ExposureAuto", enum_entry->value_nick);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to set \"ExposureAuto\" to %s. Return code was: %s", enum_entry->value_nick, ErrorCodeToMessage(result));
        }
        break;
    case PROP_BALANCEWHITEAUTO:
        enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_BALANCEWHITEAUTO_MODES), g_value_get_enum(value));
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"BalanceWhiteAuto\" to %s", enum_entry->value_nick);
        result = VmbFeatureEnumSet(vimbasrc->camera.handle, "BalanceWhiteAuto", enum_entry->value_nick);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to set \"BalanceWhiteAuto\" to %s. Return code was: %s", enum_entry->value_nick, ErrorCodeToMessage(result));
        }
        break;
    case PROP_GAIN:
        double_entry = g_value_get_double(value);
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"Gain\" to %f", double_entry);
        result = VmbFeatureFloatSet(vimbasrc->camera.handle, "Gain", double_entry);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to set \"Gain\" to %f. Return code was: %s", double_entry, ErrorCodeToMessage(result));
        }
        break;
    case PROP_EXPOSURETIME:
        // TODO: Workaround for cameras with legacy "ExposureTimeAbs" feature should be replaced with a general legacy feature name handling approach:
        // A static table maps each property, e.g. "exposuretime", to a list of (feature name, set function, get function) pairs,
        // e.g. [("ExposureTime", setExposureTime, getExposureTime), ("ExposureTimeAbs", setExposureTimeAbs, getExposureTimeAbs)].
        // On startup, the feature list of the connected camera obtained from VmbFeaturesList() is used to determine which set/get function to use.

        double_entry = g_value_get_double(value);
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"ExposureTime\" to %f", double_entry);
        result = VmbFeatureFloatSet(vimbasrc->camera.handle, "ExposureTime", double_entry);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to set \"ExposureTime\" to %f. Return code was: %s Setting \"ExposureTimeAbs\"", double_entry, ErrorCodeToMessage(result));
            result = VmbFeatureFloatSet(vimbasrc->camera.handle, "ExposureTimeAbs", double_entry);
            if (result == VmbErrorSuccess)
            {
                GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
            }
            else
            {
                GST_WARNING_OBJECT(vimbasrc, "Failed to set \"ExposureTimeAbs\" to %f. Return code was: %s", double_entry, ErrorCodeToMessage(result));
            }
        }
        break;
    case PROP_OFFSETX:
        int_entry = g_value_get_int(value);
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"OffsetX\" to %d", int_entry);
        result = VmbFeatureIntSet(vimbasrc->camera.handle, "OffsetX", int_entry);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to set \"OffsetX\" to value \"%d\". Return code was: %s",
                               int_entry,
                               ErrorCodeToMessage(result));
        }
        break;
    case PROP_OFFSETY:
        int_entry = g_value_get_int(value);
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"OffsetY\" to %d", int_entry);
        result = VmbFeatureIntSet(vimbasrc->camera.handle, "OffsetY", int_entry);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to set \"OffsetY\" to value \"%d\". Return code was: %s",
                               int_entry,
                               ErrorCodeToMessage(result));
        }
        break;
    case PROP_WIDTH:
        int_entry = g_value_get_int(value);
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"Width\" to %d", int_entry);
        result = VmbFeatureIntSet(vimbasrc->camera.handle, "Width", int_entry);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to set \"Width\" to value \"%d\". Return code was: %s",
                               int_entry,
                               ErrorCodeToMessage(result));
        }
        break;
    case PROP_HEIGHT:
        int_entry = g_value_get_int(value);
        GST_DEBUG_OBJECT(vimbasrc, "Setting \"Height\" to %d", int_entry);
        result = VmbFeatureIntSet(vimbasrc->camera.handle, "Height", int_entry);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to set \"Height\" to value \"%d\". Return code was: %s",
                               int_entry,
                               ErrorCodeToMessage(result));
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

    VmbError_t result;

    const char *vmbfeature_value_char;
    double vmbfeature_value_double;
    VmbInt64_t vmbfeature_value_int64;

    GST_DEBUG_OBJECT(vimbasrc, "get_property");

    switch (property_id)
    {
    case PROP_CAMERA_ID:
        g_value_set_string(value, vimbasrc->camera.id);
        break;
    case PROP_EXPOSUREAUTO:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "ExposureAuto", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"ExposureAuto\": %s", vmbfeature_value_char);
            g_value_set_enum(value, g_enum_get_value_by_nick(g_type_class_ref(GST_ENUM_EXPOSUREAUTO_MODES), vmbfeature_value_char)->value);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to read value of \"ExposureAuto\" from camera. Return code was: %s", ErrorCodeToMessage(result));
        }
        break;
    case PROP_BALANCEWHITEAUTO:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "BalanceWhiteAuto", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"BalanceWhiteAuto\": %s", vmbfeature_value_char);
            g_value_set_enum(value, g_enum_get_value_by_nick(g_type_class_ref(GST_ENUM_BALANCEWHITEAUTO_MODES), vmbfeature_value_char)->value);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to read value of \"BalanceWhiteAuto\" from camera. Return code was: %s", ErrorCodeToMessage(result));
        }
        break;
    case PROP_GAIN:
        result = VmbFeatureFloatGet(vimbasrc->camera.handle, "Gain", &vmbfeature_value_double);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"Gain\": %f", vmbfeature_value_double);
            g_value_set_double(value, vmbfeature_value_double);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to read value of \"Gain\" from camera. Return code was: %s", ErrorCodeToMessage(result));
        }
        break;
    case PROP_EXPOSURETIME:
        // TODO: Workaround for cameras with legacy "ExposureTimeAbs" feature should be replaced with a general legacy feature name handling approach:
        // See similar TODO above

        result = VmbFeatureFloatGet(vimbasrc->camera.handle, "ExposureTime", &vmbfeature_value_double);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"ExposureTime\": %f", vmbfeature_value_double);
            g_value_set_double(value, vmbfeature_value_double);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Failed to read value of \"ExposureTime\" from camera. Return code was: %s", ErrorCodeToMessage(result));
            result = VmbFeatureFloatGet(vimbasrc->camera.handle, "ExposureTimeAbs", &vmbfeature_value_double);
            if (result == VmbErrorSuccess)
            {
                GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"ExposureTimeAbs\": %f", vmbfeature_value_double);
                g_value_set_double(value, vmbfeature_value_double);
            }
            else
            {
                GST_WARNING_OBJECT(vimbasrc, "Failed to read value of \"ExposureTimeAbs\" from camera. Return code was: %s", ErrorCodeToMessage(result));
            }
        }
        break;
    case PROP_OFFSETX:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "OffsetX", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"OffsetX\": %lld", vmbfeature_value_int64);
            g_value_set_int(value, (gint)vmbfeature_value_int64);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Could not read value for \"OffsetX\". Got return code %s", ErrorCodeToMessage(result));
        }
        break;
    case PROP_OFFSETY:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "OffsetY", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"OffsetY\": %lld", vmbfeature_value_int64);
            g_value_set_int(value, (gint)vmbfeature_value_int64);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Could not read value for \"OffsetY\". Got return code %s", ErrorCodeToMessage(result));
        }
        break;
    case PROP_WIDTH:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "Width", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"Width\": %lld", vmbfeature_value_int64);
            g_value_set_int(value, (gint)vmbfeature_value_int64);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Could not read value for \"Width\". Got return code %s", ErrorCodeToMessage(result));
        }
        break;
    case PROP_HEIGHT:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "Height", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Camera returned the following value for \"Height\": %lld", vmbfeature_value_int64);
            g_value_set_int(value, (gint)vmbfeature_value_int64);
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc, "Could not read value for \"Height\". Got return code %s", ErrorCodeToMessage(result));
        }
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
    VmbInt64_t vmb_width, vmb_height;

    VmbFeatureIntGet(vimbasrc->camera.handle, "Width", &vmb_width);
    VmbFeatureIntGet(vimbasrc->camera.handle, "Height", &vmb_height);

    GValue width = G_VALUE_INIT;
    GValue height = G_VALUE_INIT;

    g_value_init(&width, G_TYPE_INT);
    g_value_init(&height, G_TYPE_INT);

    g_value_set_int(&width,
                    (gint)vmb_width);

    g_value_set_int(&height,
                    (gint)vmb_height);

    GstStructure *raw_caps = gst_caps_get_structure(caps, 0);
    GstStructure *bayer_caps = gst_caps_get_structure(caps, 1);

    gst_structure_set_value(raw_caps,
                            "width",
                            &width);
    gst_structure_set_value(raw_caps,
                            "height",
                            &height);
    gst_structure_set(raw_caps,
                      // TODO: Check if framerate should also be gotten from camera (e.g. as max-framerate here)
                      // Mark the framerate as variable because triggering might cause variable framerate
                      "framerate", GST_TYPE_FRACTION, 0, 1,
                      NULL);

    gst_structure_set_value(bayer_caps,
                            "width",
                            &width);
    gst_structure_set_value(bayer_caps,
                            "height",
                            &height);
    gst_structure_set(bayer_caps,
                      // TODO: Check if framerate should also be gotten from camera (e.g. as max-framerate here)
                      // Mark the framerate as variable because triggering might cause variable framerate
                      "framerate", GST_TYPE_FRACTION, 0, 1,
                      NULL);

    // Query supported pixel formats from camera and map them to GStreamer formats
    GValue pixel_format_raw_list = G_VALUE_INIT;
    g_value_init(&pixel_format_raw_list, GST_TYPE_LIST);

    GValue pixel_format_bayer_list = G_VALUE_INIT;
    g_value_init(&pixel_format_bayer_list, GST_TYPE_LIST);

    GValue pixel_format = G_VALUE_INIT;
    g_value_init(&pixel_format, G_TYPE_STRING);

    // Add all supported GStreamer format string to the reported caps
    for (unsigned int i = 0; i < vimbasrc->camera.supported_formats_count; i++)
    {
        g_value_set_static_string(&pixel_format, vimbasrc->camera.supported_formats[i]->gst_format_name);
        // TODO: Should this perhaps be done via a flag in vimba_gst_format_matches?
        if (starts_with(vimbasrc->camera.supported_formats[i]->vimba_format_name, "Bayer"))
        {
            gst_value_list_append_value(&pixel_format_bayer_list, &pixel_format);
        }
        else
        {
            gst_value_list_append_value(&pixel_format_raw_list, &pixel_format);
        }
    }
    gst_structure_set_value(raw_caps, "format", &pixel_format_raw_list);
    gst_structure_set_value(bayer_caps, "format", &pixel_format_bayer_list);

    GST_DEBUG_OBJECT(vimbasrc, "returning caps: %" GST_PTR_FORMAT, caps);

    return caps;
}

/* notify the subclass of new caps */
static gboolean
gst_vimbasrc_set_caps(GstBaseSrc *src, GstCaps *caps)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "set_caps");

    GST_DEBUG_OBJECT(vimbasrc, "caps requested to be set: %" GST_PTR_FORMAT, caps);

    // TODO: save to assume that "format" is always exactly one format and not a list?
    // gst_caps_is_fixed might otherwise be a good check and gst_caps_normalize could help make sure
    // of it
    GstStructure *structure;
    structure = gst_caps_get_structure(caps, 0);
    const char *gst_format = gst_structure_get_string(structure, "format");
    GST_DEBUG_OBJECT(vimbasrc,
                     "Looking for matching vimba pixel format to GSreamer format \"%s\"",
                     gst_format);

    const char *vimba_format = NULL;
    for (unsigned int i = 0; i < vimbasrc->camera.supported_formats_count; i++)
    {
        if (strcmp(gst_format, vimbasrc->camera.supported_formats[i]->gst_format_name) == 0)
        {
            vimba_format = vimbasrc->camera.supported_formats[i]->vimba_format_name;
            GST_DEBUG_OBJECT(vimbasrc, "Found matching vimba pixel format \"%s\"", vimba_format);
            break;
        }
    }
    if (vimba_format == NULL)
    {
        GST_ERROR_OBJECT(vimbasrc,
                         "Could not find a matching vimba pixel format for GStreamer format \"%s\"",
                         gst_format);
        return FALSE;
    }

    // Apply the requested caps to appropriate camera settings
    VmbError_t result;
    // Changing the pixel format can not be done while images are acquired
    result = stop_image_acquisition(vimbasrc);

    result = VmbFeatureEnumSet(vimbasrc->camera.handle,
                               "PixelFormat",
                               vimba_format);
    if (result != VmbErrorSuccess)
    {
        GST_ERROR_OBJECT(vimbasrc,
                         "Could not set \"PixelFormat\" to \"%s\". Got return code \"%s\"",
                         vimba_format,
                         ErrorCodeToMessage(result));
        return FALSE;
    }

    // width and height are always the value that is already written on the camera because get_caps
    // only reports that value. Setting it here is not necessary as the feature values are
    // controlled via properties of the element.

    // Buffer size needs to be increased if the new payload size is greater than the old one because
    // that means the previously allocated buffers are not large enough. We simply check the size of
    // the first buffer because they were all allocated with the same size
    VmbInt64_t new_payload_size;
    result = VmbFeatureIntGet(vimbasrc->camera.handle, "PayloadSize", &new_payload_size);
    if (vimbasrc->frame_buffers[0].bufferSize < new_payload_size || result != VmbErrorSuccess)
    {
        // Also reallocate buffers if PayloadSize could not be read because it might have increased
        GST_DEBUG_OBJECT(vimbasrc,
                         "PayloadSize increased. Reallocating frame buffers to ensure enough space");
        revoke_and_free_buffers(vimbasrc);
        result = alloc_and_announce_buffers(vimbasrc);
    }
    if (result == VmbErrorSuccess)
    {
        result = start_image_acquisition(vimbasrc);
    }

    return result == VmbErrorSuccess ? TRUE : FALSE;
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

    VmbError_t result = alloc_and_announce_buffers(vimbasrc);
    if (result == VmbErrorSuccess)
    {
        result = start_image_acquisition(vimbasrc);
    }

    // Is this necessary?
    if (result == VmbErrorSuccess)
    {
        gst_base_src_start_complete(src, GST_FLOW_OK);
    }
    else
    {
        GST_ERROR_OBJECT(vimbasrc, "Could not start acquisition. Experienced error: %s", ErrorCodeToMessage(result));
        gst_base_src_start_complete(src, GST_FLOW_ERROR);
    }

    // TODO: Is this enough error handling?
    return result == VmbErrorSuccess ? TRUE : FALSE;
}

static gboolean
gst_vimbasrc_stop(GstBaseSrc *src)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "stop");

    stop_image_acquisition(vimbasrc);

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

    if (frame->receiveStatus == VmbFrameStatusIncomplete)
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Received frame with ID \"%llu\" was incomplete", frame->frameID);
    }

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

VmbError_t alloc_and_announce_buffers(GstVimbaSrc *vimbasrc)
{
    VmbInt64_t payload_size;
    VmbError_t result = VmbFeatureIntGet(vimbasrc->camera.handle, "PayloadSize", &payload_size);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Got \"PayloadSize\" of: %llu", payload_size);
        GST_DEBUG_OBJECT(vimbasrc, "Allocating and announcing %d vimba frames", NUM_VIMBA_FRAMES);
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
    }
    return result;
}

void revoke_and_free_buffers(GstVimbaSrc *vimbasrc)
{
    for (int i = 0; i < NUM_VIMBA_FRAMES; i++)
    {
        if (NULL != vimbasrc->frame_buffers[i].buffer)
        {
            VmbFrameRevoke(vimbasrc->camera.handle, &vimbasrc->frame_buffers[i]);
            free(vimbasrc->frame_buffers[i].buffer);
            memset(&vimbasrc->frame_buffers[i], 0, sizeof(VmbFrame_t));
        }
    }
}

VmbError_t start_image_acquisition(GstVimbaSrc *vimbasrc)
{
    // Start Capture Engine
    GST_DEBUG_OBJECT(vimbasrc, "Starting the capture engine");
    VmbError_t result = VmbCaptureStart(vimbasrc->camera.handle);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Queueing the vimba frames");
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
            GST_DEBUG_OBJECT(vimbasrc, "Running \"AcquisitionStart\" feature");
            result = VmbFeatureCommandRun(vimbasrc->camera.handle, "AcquisitionStart");
        }
    }
    return result;
}

VmbError_t stop_image_acquisition(GstVimbaSrc *vimbasrc)
{
    // Stop Acquisition
    GST_DEBUG_OBJECT(vimbasrc, "Running \"AcquisitionStop\" feature");
    VmbError_t result = VmbFeatureCommandRun(vimbasrc->camera.handle, "AcquisitionStop");

    // Stop Capture Engine
    GST_DEBUG_OBJECT(vimbasrc, "Stopping the capture engine");
    result = VmbCaptureEnd(vimbasrc->camera.handle);

    // Flush the capture queue
    GST_DEBUG_OBJECT(vimbasrc, "Flushing the capture queue");
    VmbCaptureQueueFlush(vimbasrc->camera.handle);

    return result;
}

void VMB_CALL vimba_frame_callback(const VmbHandle_t camera_handle, VmbFrame_t *frame)
{
    GST_DEBUG("Got Frame");
    g_async_queue_push(g_filled_frame_queue, frame);

    // requeueing the frame is done after it was consumed in vimbasrc_create
}

void query_supported_pixel_formats(GstVimbaSrc *vimbasrc)
{
    // get number of supported formats from the camera
    VmbUint32_t camera_format_count;
    VmbFeatureEnumRangeQuery(
        vimbasrc->camera.handle,
        "PixelFormat",
        NULL,
        0,
        &camera_format_count);

    // get the vimba format string supported by the camera
    const char **supported_formats = malloc(camera_format_count * sizeof(char *));
    VmbFeatureEnumRangeQuery(
        vimbasrc->camera.handle,
        "PixelFormat",
        supported_formats,
        camera_format_count,
        NULL);

    GST_DEBUG_OBJECT(vimbasrc, "Got %d supported formats", camera_format_count);
    for (unsigned int i = 0; i < camera_format_count; i++)
    {
        const VimbaGstFormatMatch_t *format_map = gst_format_from_vimba_format(supported_formats[i]);
        if (format_map != NULL)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Vimba format \"%s\" corresponds to GStreamer format \"%s\"",
                             supported_formats[i],
                             format_map->gst_format_name);
            vimbasrc->camera.supported_formats[vimbasrc->camera.supported_formats_count] = format_map;
            vimbasrc->camera.supported_formats_count++;
        }
        else
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "No corresponding GStreamer format found for vimba format \"%s\"",
                             supported_formats[i]);
        }
    }
    free(supported_formats);
}
