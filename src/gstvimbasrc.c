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

static void gst_vimbasrc_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_vimbasrc_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
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
    PROP_EXPOSURETIME,
    PROP_EXPOSUREAUTO,
    PROP_BALANCEWHITEAUTO,
    PROP_GAIN,
    PROP_OFFSETX,
    PROP_OFFSETY,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_TRIGGERSELECTOR,
    PROP_TRIGGERMODE,
    PROP_TRIGGERSOURCE,
    PROP_TRIGGERACTIVATION,
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
static GType gst_vimbasrc_exposureauto_get_type(void)
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
static GType gst_vimbasrc_balancewhiteauto_get_type(void)
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

/* TriggerSelector values */
// TODO: Which of these are really needed?
#define GST_ENUM_TRIGGERSELECTOR_VALUES (gst_vimbasrc_triggerselector_get_type())
static GType gst_vimbasrc_triggerselector_get_type(void)
{
    static GType vimbasrc_triggerselector_type = 0;
    static const GEnumValue triggerselector_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the Vimba FeatureEnum */
        {GST_VIMBASRC_TRIGGERSELECTOR_ACQUISITION_START, "Selects a trigger that starts the Acquisition of one or many frames according to AcquisitionMode", "AcquisitionStart"},
        {GST_VIMBASRC_TRIGGERSELECTOR_ACQUISITION_END, "Selects a trigger that ends the Acquisition of one or many frames according to AcquisitionMode", "AcquisitionEnd"},
        {GST_VIMBASRC_TRIGGERSELECTOR_ACQUISITION_ACTIVE, "Selects a trigger that controls the duration of the Acquisition of one or many frames. The Acquisition is activated when the trigger signal becomes active and terminated when it goes back to the inactive state", "AcquisitionActive"},
        {GST_VIMBASRC_TRIGGERSELECTOR_FRAME_START, "Selects a trigger starting the capture of one frame", "FrameStart"},
        {GST_VIMBASRC_TRIGGERSELECTOR_FRAME_END, "Selects a trigger ending the capture of one frame (mainly used in linescanmode)", "FrameEnd"},
        {GST_VIMBASRC_TRIGGERSELECTOR_FRAME_ACTIVE, "Selects a trigger controlling the duration of one frame (mainly used in linescanmode)", "FrameActive"},
        {GST_VIMBASRC_TRIGGERSELECTOR_FRAME_BURST_START, "Selects a trigger starting the capture of the bursts of frames in an acquisition. AcquisitionBurstFrameCount controls the length of each burst unless a FrameBurstEnd trigger is active. The total number of frames captured is also conditioned by AcquisitionFrameCount if AcquisitionMode is MultiFrame", "FrameBurstStart"},
        {GST_VIMBASRC_TRIGGERSELECTOR_FRAME_BURST_END, "Selects a trigger ending the capture of the bursts of frames in an acquisition", "FrameBurstEnd"},
        {GST_VIMBASRC_TRIGGERSELECTOR_FRAME_BURST_ACTIVE, "Selects a trigger controlling the duration of the capture of the bursts of frames in an acquisition", "FrameBurstActive"},
        {GST_VIMBASRC_TRIGGERSELECTOR_LINE_START, "Selects a trigger starting the capture of one Line of a Frame (mainly used in linescanmode)", "LineStart"},
        {GST_VIMBASRC_TRIGGERSELECTOR_EXPOSURE_START, "Selects a trigger controlling the start of the exposure of one Frame (or Line)", "ExposureStart"},
        {GST_VIMBASRC_TRIGGERSELECTOR_EXPOSURE_END, "Selects a trigger controlling the end of the exposure of one Frame (or Line)", "ExposureEnd"},
        {GST_VIMBASRC_TRIGGERSELECTOR_EXPOSURE_ACTIVE, "Selects a trigger controlling the duration of the exposure of one frame (or Line)", "ExposureActive"},
        {GST_VIMBASRC_TRIGGERSELECTOR_MULTI_SLOPE_EXPOSURE_LIMIT1, "Selects a trigger controlling the first duration of a multi-slope exposure. Exposure is continued according to the pre-defined multi-slope settings", "MultiSlopeExposureLimit1"},
        {0, NULL, NULL}};
    if (!vimbasrc_triggerselector_type)
    {
        vimbasrc_triggerselector_type =
            g_enum_register_static("GstVimbasrcTriggerSelectorValues", triggerselector_values);
    }
    return vimbasrc_triggerselector_type;
}

/* TriggerMode values */
#define GST_ENUM_TRIGGERMODE_VALUES (gst_vimbasrc_triggermode_get_type())
static GType gst_vimbasrc_triggermode_get_type(void)
{
    static GType vimbasrc_triggermode_type = 0;
    static const GEnumValue triggermode_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the Vimba FeatureEnum */
        {GST_VIMBASRC_TRIGGERMODE_OFF, "Disables the selected trigger", "Off"},
        {GST_VIMBASRC_TRIGGERMODE_ON, "Enable the selected trigger", "On"},
        {0, NULL, NULL}};
    if (!vimbasrc_triggermode_type)
    {
        vimbasrc_triggermode_type =
            g_enum_register_static("GstVimbasrcTriggerModeValues", triggermode_values);
    }
    return vimbasrc_triggermode_type;
}

/* TriggerSource values */
// TODO: which of these are really needed? Current entries taken from SFNC
#define GST_ENUM_TRIGGERSOURCE_VALUES (gst_vimbasrc_triggersource_get_type())
static GType gst_vimbasrc_triggersource_get_type(void)
{
    static GType vimbasrc_triggersource_type = 0;
    static const GEnumValue triggersource_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the Vimba FeatureEnum */
        {GST_VIMBASRC_TRIGGERSOURCE_SOFTWARE, "Specifies that the trigger source will be generated by software using the TriggerSoftware command", "Software"},
        {GST_VIMBASRC_TRIGGERSOURCE_SOFTWARE_SIGNAL0, "Specifies that the trigger source will be a signal generated by software using the SoftwareSignalPulse command", "SoftwareSignal0"},
        {GST_VIMBASRC_TRIGGERSOURCE_LINE0, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line0"},
        {GST_VIMBASRC_TRIGGERSOURCE_LINE1, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line1"},
        {GST_VIMBASRC_TRIGGERSOURCE_LINE2, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line2"},
        {GST_VIMBASRC_TRIGGERSOURCE_LINE3, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line3"},
        {GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT0, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput0"},
        {GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT1, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput1"},
        {GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT2, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput2"},
        {GST_VIMBASRC_TRIGGERSOURCE_USER_OUTPUT3, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput3"},
        {GST_VIMBASRC_TRIGGERSOURCE_COUNTER0_START, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter0Start"},
        {GST_VIMBASRC_TRIGGERSOURCE_COUNTER0_END, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter0End"},
        {GST_VIMBASRC_TRIGGERSOURCE_TIMER0_START, "Specifies which Timer signal to use as internal source for the trigger", "Timer0Start"},
        {GST_VIMBASRC_TRIGGERSOURCE_TIMER0_END, "Specifies which Timer signal to use as internal source for the trigger", "Timer0End"},
        {GST_VIMBASRC_TRIGGERSOURCE_ENCODER0, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder0"},
        {GST_VIMBASRC_TRIGGERSOURCE_ENCODER1, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder1"},
        {GST_VIMBASRC_TRIGGERSOURCE_ENCODER2, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder2"},
        {GST_VIMBASRC_TRIGGERSOURCE_ENCODER3, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder3"},
        {GST_VIMBASRC_TRIGGERSOURCE_LOGIC_BLOCK0, "Specifies which Logic Block signal to use as internal source for the trigger", "LogicBlock0"},
        {GST_VIMBASRC_TRIGGERSOURCE_ACTION0, "Specifies which Action command to use as internal source for the trigger", "Action0"},
        {GST_VIMBASRC_TRIGGERSOURCE_LINK_TRIGGER0, "Specifies which Link Trigger to use as source for the trigger (received from the transport layer)", "LinkTrigger0"},
        {GST_VIMBASRC_TRIGGERSOURCE_CC1, "Index of the Camera Link physical line and associated I/O control block to use. This ensures a direct mapping between the lines on the frame grabber and on the camera. Applicable to CameraLink products only", "CC1"},
        {0, NULL, NULL}};
    if (!vimbasrc_triggersource_type)
    {
        vimbasrc_triggersource_type =
            g_enum_register_static("GstVimbasrcTriggerSourceValues", triggersource_values);
    }
    return vimbasrc_triggersource_type;
}

/* TriggerActivation values */
#define GST_ENUM_TRIGGERACTIVATION_VALUES (gst_vimbasrc_triggeractivation_get_type())
static GType gst_vimbasrc_triggeractivation_get_type(void)
{
    static GType vimbasrc_triggeractivation_type = 0;
    static const GEnumValue triggeractivation_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the Vimba FeatureEnum */
        {GST_VIMBASRC_TRIGGERACTIVATION_RISING_EDGE, "Specifies that the trigger is considered valid on the rising edge of the source signal", "RisingEdge"},
        {GST_VIMBASRC_TRIGGERACTIVATION_FALLING_EDGE, "Specifies that the trigger is considered valid on the falling edge of the source signal", "FallingEdge"},
        {GST_VIMBASRC_TRIGGERACTIVATION_ANY_EDGE, "Specifies that the trigger is considered valid on the falling or rising edge of the source signal", "AnyEdge"},
        {GST_VIMBASRC_TRIGGERACTIVATION_LEVEL_HIGH, "Specifies that the trigger is considered valid as long as the level of the source signal is high", "LevelHigh"},
        {GST_VIMBASRC_TRIGGERACTIVATION_LEVEL_LOW, "Specifies that the trigger is considered valid as long as the level of the source signal is low", "LevelLow"},
        {0, NULL, NULL}};
    if (!vimbasrc_triggeractivation_type)
    {
        vimbasrc_triggeractivation_type =
            g_enum_register_static("GstVimbasrcTriggerActivationValues", triggeractivation_values);
    }
    return vimbasrc_triggeractivation_type;
}

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstVimbaSrc,
                        gst_vimbasrc,
                        GST_TYPE_PUSH_SRC,
                        GST_DEBUG_CATEGORY_INIT(gst_vimbasrc_debug_category,
                                                "vimbasrc",
                                                0,
                                                "debug category for vimbasrc element"));

static void gst_vimbasrc_class_init(GstVimbaSrcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
                                              &gst_vimbasrc_src_template);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          "Vimba GStreamer source",
                                          "Generic",
                                          DESCRIPTION,
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
            "Width of the image provided by the device (in pixels). If no explicit value is passed the full sensor width is used.",
            0,
            G_MAXINT,
            G_MAXINT,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_HEIGHT,
        g_param_spec_int(
            "height",
            "Height feature setting",
            "Height of the image provided by the device (in pixels). If no explicit value is passed the full sensor height is used.",
            0,
            G_MAXINT,
            G_MAXINT,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_TRIGGERSELECTOR,
        g_param_spec_enum(
            "triggerselector",
            "TriggerSelector feature setting",
            "Selects the type of trigger to configure",
            GST_ENUM_TRIGGERSELECTOR_VALUES,
            GST_VIMBASRC_TRIGGERSELECTOR_ACQUISITION_START,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_TRIGGERMODE,
        g_param_spec_enum(
            "triggermode",
            "TriggerMode feature setting",
            "Controls if the selected trigger is active",
            GST_ENUM_TRIGGERMODE_VALUES,
            GST_VIMBASRC_TRIGGERMODE_OFF,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_TRIGGERSOURCE,
        g_param_spec_enum(
            "triggersource",
            "TriggerSource feature setting",
            "Specifies the internal signal or physical input Line to use as the trigger source. The selected trigger must have its TriggerMode set to On",
            GST_ENUM_TRIGGERSOURCE_VALUES,
            GST_VIMBASRC_TRIGGERSOURCE_SOFTWARE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_TRIGGERACTIVATION,
        g_param_spec_enum(
            "triggeractivation",
            "TriggerActivation feature setting",
            "Specifies the activation mode of the trigger",
            GST_ENUM_TRIGGERACTIVATION_VALUES,
            GST_VIMBASRC_TRIGGERACTIVATION_RISING_EDGE,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_vimbasrc_init(GstVimbaSrc *vimbasrc)
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
        GST_INFO_OBJECT(vimbasrc,
                        "Running with VimbaC Version %u.%u.%u",
                        version_info.major,
                        version_info.minor,
                        version_info.patch);
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

    // Set property helper variables to default values
    GObjectClass *gobject_class = G_OBJECT_GET_CLASS(vimbasrc);

    vimbasrc->properties.camera_id = g_value_dup_string(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "camera")));
    vimbasrc->properties.exposuretime = g_value_get_double(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "exposuretime")));
    vimbasrc->properties.exposureauto = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "exposureauto")));
    vimbasrc->properties.balancewhiteauto = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "balancewhiteauto")));
    vimbasrc->properties.gain = g_value_get_double(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "gain")));
    vimbasrc->properties.offsetx = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "offsetx")));
    vimbasrc->properties.offsety = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "offsety")));
    vimbasrc->properties.width = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "width")));
    vimbasrc->properties.height = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "height")));
    vimbasrc->properties.triggerselector = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggerselector")));
    vimbasrc->properties.triggermode = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggermode")));
    vimbasrc->properties.triggersource = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggersource")));
    vimbasrc->properties.triggeractivation = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggeractivation")));
}

void gst_vimbasrc_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(object);

    GST_DEBUG_OBJECT(vimbasrc, "set_property");

    switch (property_id)
    {
    case PROP_CAMERA_ID:
        free((void *)vimbasrc->camera.id); // Free memory of old entry
        vimbasrc->camera.id = g_value_dup_string(value);
        break;
    case PROP_EXPOSURETIME:
        vimbasrc->properties.exposuretime = g_value_get_double(value);
        break;
    case PROP_EXPOSUREAUTO:
        vimbasrc->properties.exposureauto = g_value_get_enum(value);
        break;
    case PROP_BALANCEWHITEAUTO:
        vimbasrc->properties.balancewhiteauto = g_value_get_enum(value);
        break;
    case PROP_GAIN:
        vimbasrc->properties.gain = g_value_get_double(value);
        break;
    case PROP_OFFSETX:
        vimbasrc->properties.offsetx = g_value_get_int(value);
        break;
    case PROP_OFFSETY:
        vimbasrc->properties.offsety = g_value_get_int(value);
        break;
    case PROP_WIDTH:
        vimbasrc->properties.width = g_value_get_int(value);
        break;
    case PROP_HEIGHT:
        vimbasrc->properties.height = g_value_get_int(value);
        break;
    case PROP_TRIGGERSELECTOR:
        vimbasrc->properties.triggerselector = g_value_get_enum(value);
        break;
    case PROP_TRIGGERMODE:
        vimbasrc->properties.triggermode = g_value_get_enum(value);
        break;
    case PROP_TRIGGERSOURCE:
        vimbasrc->properties.triggersource = g_value_get_enum(value);
        break;
    case PROP_TRIGGERACTIVATION:
        vimbasrc->properties.triggeractivation = g_value_get_enum(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_vimbasrc_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
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
    case PROP_EXPOSURETIME:
        // TODO: Workaround for cameras with legacy "ExposureTimeAbs" feature should be replaced with a general legacy
        // feature name handling approach: See similar TODO above

        result = VmbFeatureFloatGet(vimbasrc->camera.handle, "ExposureTime", &vmbfeature_value_double);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"ExposureTime\": %f",
                             vmbfeature_value_double);
            vimbasrc->properties.exposuretime = vmbfeature_value_double;
        }
        else if (result == VmbErrorNotFound)
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to get \"ExposureTime\". Return code was: %s Attempting \"ExposureTimeAbs\"",
                               ErrorCodeToMessage(result));
            result = VmbFeatureFloatGet(vimbasrc->camera.handle, "ExposureTimeAbs", &vmbfeature_value_double);
            if (result == VmbErrorSuccess)
            {
                GST_DEBUG_OBJECT(vimbasrc,
                                 "Camera returned the following value for \"ExposureTimeAbs\": %f",
                                 vmbfeature_value_double);
                vimbasrc->properties.exposuretime = vmbfeature_value_double;
            }
            else
            {
                GST_WARNING_OBJECT(vimbasrc,
                                   "Failed to read value of \"ExposureTimeAbs\" from camera. Return code was: %s",
                                   ErrorCodeToMessage(result));
            }
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"ExposureTime\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }

        g_value_set_double(value, vimbasrc->properties.exposuretime);
        break;
    case PROP_EXPOSUREAUTO:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "ExposureAuto", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"ExposureAuto\": %s",
                             vmbfeature_value_char);
            vimbasrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                    g_type_class_ref(GST_ENUM_EXPOSUREAUTO_MODES),
                                                    vmbfeature_value_char)
                                                    ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"ExposureAuto\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbasrc->properties.exposureauto);
        break;
    case PROP_BALANCEWHITEAUTO:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "BalanceWhiteAuto", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"BalanceWhiteAuto\": %s",
                             vmbfeature_value_char);
            vimbasrc->properties.balancewhiteauto = g_enum_get_value_by_nick(
                                                        g_type_class_ref(GST_ENUM_BALANCEWHITEAUTO_MODES),
                                                        vmbfeature_value_char)
                                                        ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"BalanceWhiteAuto\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbasrc->properties.balancewhiteauto);
        break;
    case PROP_GAIN:
        result = VmbFeatureFloatGet(vimbasrc->camera.handle, "Gain", &vmbfeature_value_double);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"Gain\": %f",
                             vmbfeature_value_double);
            vimbasrc->properties.gain = vmbfeature_value_double;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"Gain\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_double(value, vimbasrc->properties.gain);
        break;
    case PROP_OFFSETX:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "OffsetX", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"OffsetX\": %lld",
                             vmbfeature_value_int64);
            vimbasrc->properties.offsetx = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Could not read value for \"OffsetX\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbasrc->properties.offsetx);
        break;
    case PROP_OFFSETY:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "OffsetY", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"OffsetY\": %lld",
                             vmbfeature_value_int64);
            vimbasrc->properties.offsety = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Could not read value for \"OffsetY\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbasrc->properties.offsety);
        break;
    case PROP_WIDTH:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "Width", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"Width\": %lld",
                             vmbfeature_value_int64);
            vimbasrc->properties.width = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Could not read value for \"Width\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbasrc->properties.width);
        break;
    case PROP_HEIGHT:
        result = VmbFeatureIntGet(vimbasrc->camera.handle, "Height", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"Height\": %lld",
                             vmbfeature_value_int64);
            vimbasrc->properties.height = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Could not read value for \"Height\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbasrc->properties.height);
        break;
    case PROP_TRIGGERSELECTOR:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "TriggerSelector", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"TriggerSelector\": %s",
                             vmbfeature_value_char);
            vimbasrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                    g_type_class_ref(GST_ENUM_TRIGGERSELECTOR_VALUES),
                                                    vmbfeature_value_char)
                                                    ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"TriggerSelector\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbasrc->properties.triggerselector);
        break;
    case PROP_TRIGGERMODE:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "TriggerMode", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"TriggerMode\": %s",
                             vmbfeature_value_char);
            vimbasrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                    g_type_class_ref(GST_ENUM_TRIGGERMODE_VALUES),
                                                    vmbfeature_value_char)
                                                    ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"TriggerMode\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbasrc->properties.triggermode);
        break;
    case PROP_TRIGGERSOURCE:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "TriggerSource", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"TriggerSource\": %s",
                             vmbfeature_value_char);
            vimbasrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                    g_type_class_ref(GST_ENUM_TRIGGERSOURCE_VALUES),
                                                    vmbfeature_value_char)
                                                    ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"TriggerSource\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbasrc->properties.triggersource);
        break;
    case PROP_TRIGGERACTIVATION:
        result = VmbFeatureEnumGet(vimbasrc->camera.handle, "TriggerActivation", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc,
                             "Camera returned the following value for \"TriggerActivation\": %s",
                             vmbfeature_value_char);
            vimbasrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                    g_type_class_ref(GST_ENUM_TRIGGERACTIVATION_VALUES),
                                                    vmbfeature_value_char)
                                                    ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to read value of \"TriggerActivation\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbasrc->properties.triggeractivation);
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
        GST_ERROR_OBJECT(vimbasrc,
                         "Closing camera %s failed. Got error code: %s",
                         vimbasrc->camera.id,
                         ErrorCodeToMessage(result));
    }
    vimbasrc->camera.is_connected = false;

    VmbShutdown();
    GST_DEBUG_OBJECT(vimbasrc, "Vimba API was shut down");

    G_OBJECT_CLASS(gst_vimbasrc_parent_class)->finalize(object);
}

/* get caps from subclass */
static GstCaps *gst_vimbasrc_get_caps(GstBaseSrc *src, GstCaps *filter)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "get_caps");

    GstCaps *caps;
    caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    caps = gst_caps_make_writable(caps);

    // Query the capabilities from the camera and return sensible values. If no camera is connected the template caps
    // are returned
    if (vimbasrc->camera.is_connected)
    {
        VmbInt64_t vmb_width, vmb_height;

        VmbFeatureIntGet(vimbasrc->camera.handle, "Width", &vmb_width);
        VmbFeatureIntGet(vimbasrc->camera.handle, "Height", &vmb_height);

        GValue width = G_VALUE_INIT;
        GValue height = G_VALUE_INIT;

        g_value_init(&width, G_TYPE_INT);
        g_value_init(&height, G_TYPE_INT);

        g_value_set_int(&width, (gint)vmb_width);

        g_value_set_int(&height, (gint)vmb_height);

        GstStructure *raw_caps = gst_caps_get_structure(caps, 0);
        GstStructure *bayer_caps = gst_caps_get_structure(caps, 1);

        gst_structure_set_value(raw_caps, "width", &width);
        gst_structure_set_value(raw_caps, "height", &height);
        gst_structure_set(raw_caps,
                          // TODO: Check if framerate should also be gotten from camera (e.g. as max-framerate here)
                          // Mark the framerate as variable because triggering might cause variable framerate
                          "framerate", GST_TYPE_FRACTION, 0, 1,
                          NULL);

        gst_structure_set_value(bayer_caps, "width", &width);
        gst_structure_set_value(bayer_caps, "height", &height);
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
    }

    GST_DEBUG_OBJECT(vimbasrc, "returning caps: %" GST_PTR_FORMAT, caps);

    return caps;
}

/* notify the subclass of new caps */
static gboolean gst_vimbasrc_set_caps(GstBaseSrc *src, GstCaps *caps)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "set_caps");

    GST_DEBUG_OBJECT(vimbasrc, "caps requested to be set: %" GST_PTR_FORMAT, caps);

    // TODO: save to assume that "format" is always exactly one format and not a list? gst_caps_is_fixed might otherwise
    // be a good check and gst_caps_normalize could help make sure of it
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

    // width and height are always the value that is already written on the camera because get_caps only reports that
    // value. Setting it here is not necessary as the feature values are controlled via properties of the element.

    // Buffer size needs to be increased if the new payload size is greater than the old one because that means the
    // previously allocated buffers are not large enough. We simply check the size of the first buffer because they were
    // all allocated with the same size
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
static gboolean gst_vimbasrc_start(GstBaseSrc *src)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "start");

    /* TODO:
        - Clarify how Hardware triggering influences the setup required here
    */

    // Prepare queue for filled frames from which vimbasrc_create can take them
    g_filled_frame_queue = g_async_queue_new();

    VmbError_t result;

    // TODO: Error handling
    if (!vimbasrc->camera.is_connected)
    {
        result = open_camera_connection(vimbasrc);
        if (result != VmbErrorSuccess)
        {
            // Can't connect to camera. Abort execution by returning FALSE. This stops the pipeline!
            return FALSE;
        }
    }

    result = apply_feature_settings(vimbasrc);

    result = alloc_and_announce_buffers(vimbasrc);
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

static gboolean gst_vimbasrc_stop(GstBaseSrc *src)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "stop");

    stop_image_acquisition(vimbasrc);

    revoke_and_free_buffers(vimbasrc);

    // Unref the filled frame queue so it is deleted properly
    g_async_queue_unref(g_filled_frame_queue);

    return TRUE;
}

/* ask the subclass to create a buffer */
static GstFlowReturn gst_vimbasrc_create(GstPushSrc *src, GstBuffer **buf)
{
    GstVimbaSrc *vimbasrc = GST_vimbasrc(src);

    GST_DEBUG_OBJECT(vimbasrc, "create");

    // Wait until we can get a filled frame (added to queue in vimba_frame_callback)
    // TODO: Use g_async_queue_timeout_pop and check for state change to see if an early exit is desired
    VmbFrame_t *frame = NULL;
    GstStateChangeReturn ret;
    GstState state;
    do
    {
        // Try to get a filled frame for 10 microseconds
        frame = g_async_queue_timeout_pop(g_filled_frame_queue, 10);
        // Get the current state of the element. Should return immediately since we are not doing ASYNC state changes
        // but wait at most for 100 nanoseconds
        ret = gst_element_get_state(GST_ELEMENT(vimbasrc), &state, NULL, 100); // timeout is given in nanoseconds
        if (ret == GST_STATE_CHANGE_SUCCESS && state != GST_STATE_PLAYING)
        {
            // The src should not create any more data. Stop waiting for frame and do not fill buf
            // TODO: Is this the correct retrun value in this case?
            return GST_FLOW_FLUSHING;
        }
    } while (frame == NULL);

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

static gboolean plugin_init(GstPlugin *plugin)
{

    /* FIXME Remember to set the rank if it's an element that is meant to be autoplugged by decodebin. */
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

/**
 * @brief Opens the connection to the camera given by the ID passed as vimbasrc property and stores the resulting handle
 *
 * @param vimbasrc Provides access to the camera ID and holds the resulting handle
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t open_camera_connection(GstVimbaSrc *vimbasrc)
{
    VmbError_t result = VmbCameraOpen(vimbasrc->camera.id, VmbAccessModeFull, &vimbasrc->camera.handle);
    if (result == VmbErrorSuccess)
    {
        GST_INFO_OBJECT(vimbasrc, "Successfully opened camera %s", vimbasrc->camera.id);
        vimbasrc->camera.is_connected = true;
        map_supported_pixel_formats(vimbasrc);
    }
    else
    {
        GST_ERROR_OBJECT(vimbasrc,
                         "Could not open camera %s. Got error code: %s",
                         vimbasrc->camera.id,
                         ErrorCodeToMessage(result));
        vimbasrc->camera.is_connected = false;
        // TODO: List available cameras in this case?
        // TODO: Can we signal an error to the pipeline to stop immediately?
    }
    vimbasrc->camera.is_acquiring = false;
    return result;
}

/**
 * @brief Applies the values defiend in the vimbasrc properties to their corresponding Vimba camera features
 *
 * @param vimbasrc Provides access to the camera handle used for the Vimba calls and holds the desired values for the
 * modified features
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t apply_feature_settings(GstVimbaSrc *vimbasrc)
{
    bool was_acquiring = vimbasrc->camera.is_acquiring;
    if (vimbasrc->camera.is_acquiring)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Camera was acquiring. Stopping to change feature settings");
        stop_image_acquisition(vimbasrc);
    }
    GEnumValue *enum_entry;

    // exposure time
    // TODO: Workaround for cameras with legacy "ExposureTimeAbs" feature should be replaced with a general legacy
    // feature name handling approach: A static table maps each property, e.g. "exposuretime", to a list of (feature
    // name, set function, get function) pairs, e.g. [("ExposureTime", setExposureTime, getExposureTime),
    // ("ExposureTimeAbs", setExposureTimeAbs, getExposureTimeAbs)]. On startup, the feature list of the connected
    // camera obtained from VmbFeaturesList() is used to determine which set/get function to use.

    GST_DEBUG_OBJECT(vimbasrc, "Setting \"ExposureTime\" to %f", vimbasrc->properties.exposuretime);
    VmbError_t result = VmbFeatureFloatSet(vimbasrc->camera.handle, "ExposureTime", vimbasrc->properties.exposuretime);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else if (result == VmbErrorNotFound)
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"ExposureTime\" to %f. Return code was: %s Attempting \"ExposureTimeAbs\"",
                           vimbasrc->properties.exposuretime,
                           ErrorCodeToMessage(result));
        result = VmbFeatureFloatSet(vimbasrc->camera.handle, "ExposureTimeAbs", vimbasrc->properties.exposuretime);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbasrc,
                               "Failed to set \"ExposureTimeAbs\" to %f. Return code was: %s",
                               vimbasrc->properties.exposuretime,
                               ErrorCodeToMessage(result));
        }
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"ExposureTime\" to %f. Return code was: %s",
                           vimbasrc->properties.exposuretime,
                           ErrorCodeToMessage(result));
    }

    // Exposure Auto
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_EXPOSUREAUTO_MODES), vimbasrc->properties.exposureauto);
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"ExposureAuto\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbasrc->camera.handle, "ExposureAuto", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"ExposureAuto\" to %s. Return code was: %s",
                           enum_entry->value_nick,
                           ErrorCodeToMessage(result));
    }

    // Auto whitebalance
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_BALANCEWHITEAUTO_MODES),
                                  vimbasrc->properties.balancewhiteauto);
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"BalanceWhiteAuto\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbasrc->camera.handle, "BalanceWhiteAuto", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"BalanceWhiteAuto\" to %s. Return code was: %s",
                           enum_entry->value_nick,
                           ErrorCodeToMessage(result));
    }

    // gain
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"Gain\" to %f", vimbasrc->properties.gain);
    result = VmbFeatureFloatSet(vimbasrc->camera.handle, "Gain", vimbasrc->properties.gain);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"Gain\" to %f. Return code was: %s",
                           vimbasrc->properties.gain,
                           ErrorCodeToMessage(result));
    }

    result = set_roi(vimbasrc);

    result = apply_trigger_settings(vimbasrc);

    if (was_acquiring)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Camera was acquiring before changing feature settings. Restarting.");
        result = start_image_acquisition(vimbasrc);
    }

    return result;
}

/**
 * @brief Helper function to set Width, Height, OffsetX and OffsetY feature in correct order to define the region of
 * interest (ROI) on the sensor.
 *
 * The values for setting the ROI are defined as GStreamer properties of the vimbasrc element. If INT_MAX are used for
 * the width/height property (the default value) the full corresponding sensor size for that feature is used.
 *
 * @param vimbasrc Provides access to the camera handle used for the Vimba calls and holds the desired values for the
 * modified features
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t set_roi(GstVimbaSrc *vimbasrc)
{
    // TODO: Improve error handling (Perhaps more explicit allowed values are enough?) Early exit on errors?

    // Reset OffsetX and OffsetY to 0 so that full sensor width is usable for width/height
    VmbError_t result;
    GST_DEBUG_OBJECT(vimbasrc, "Temporarily resetting \"OffsetX\" and \"OffsetY\" to 0");
    result = VmbFeatureIntSet(vimbasrc->camera.handle, "OffsetX", 0);
    if (result != VmbErrorSuccess)
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"OffsetX\" to 0. Return code was: %s",
                           ErrorCodeToMessage(result));
    }
    result = VmbFeatureIntSet(vimbasrc->camera.handle, "OffsetY", 0);
    if (result != VmbErrorSuccess)
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"OffsetY\" to 0. Return code was: %s",
                           ErrorCodeToMessage(result));
    }

    // Set Width to full sensor if no explicit width was set
    if (vimbasrc->properties.width == INT_MAX)
    {
        VmbInt64_t vmb_width;
        result = VmbFeatureIntRangeQuery(vimbasrc->camera.handle, "Width", NULL, &vmb_width);
        GST_DEBUG_OBJECT(vimbasrc,
                         "Setting \"Width\" to full width. Got sensor width \"%lld\" (Return Code %s)",
                         vmb_width,
                         ErrorCodeToMessage(result));
        g_object_set(vimbasrc, "width", (int)vmb_width, NULL);
    }
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"Width\" to %d", vimbasrc->properties.width);
    result = VmbFeatureIntSet(vimbasrc->camera.handle, "Width", vimbasrc->properties.width);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"Width\" to value \"%d\". Return code was: %s",
                           vimbasrc->properties.width,
                           ErrorCodeToMessage(result));
    }

    // Set Height to full sensor if no explicit height was set
    if (vimbasrc->properties.height == INT_MAX)
    {
        VmbInt64_t vmb_height;
        result = VmbFeatureIntRangeQuery(vimbasrc->camera.handle, "Height", NULL, &vmb_height);
        GST_DEBUG_OBJECT(vimbasrc,
                         "Setting \"Height\" to full height. Got sensor height \"%lld\" (Return Code %s)",
                         vmb_height,
                         ErrorCodeToMessage(result));
        g_object_set(vimbasrc, "height", (int)vmb_height, NULL);
    }
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"Height\" to %d", vimbasrc->properties.height);
    result = VmbFeatureIntSet(vimbasrc->camera.handle, "Height", vimbasrc->properties.height);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"Height\" to value \"%d\". Return code was: %s",
                           vimbasrc->properties.height,
                           ErrorCodeToMessage(result));
    }
    // offsetx
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"OffsetX\" to %d", vimbasrc->properties.offsetx);
    result = VmbFeatureIntSet(vimbasrc->camera.handle, "OffsetX", vimbasrc->properties.offsetx);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"OffsetX\" to value \"%d\". Return code was: %s",
                           vimbasrc->properties.offsetx,
                           ErrorCodeToMessage(result));
    }

    // offsety
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"OffsetY\" to %d", vimbasrc->properties.offsety);
    result = VmbFeatureIntSet(vimbasrc->camera.handle, "OffsetY", vimbasrc->properties.offsety);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbasrc,
                           "Failed to set \"OffsetY\" to value \"%d\". Return code was: %s",
                           vimbasrc->properties.offsety,
                           ErrorCodeToMessage(result));
    }
    return result;
}

/**
 * @brief Helper function to apply values to TriggerSelector, TriggerMode, TriggerSource and TriggerActivation in the
 * correct order
 *
 * Trigger settings are always applied in the order
 * 1. TriggerSelector
 * 2. TriggerActivation
 * 3. TriggerSource
 * 4. TriggerMode
 *
 * @param vimbasrc Provides access to the camera handle used for the Vimba calls and holds the desired values for the
 * modified features
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t apply_trigger_settings(GstVimbaSrc *vimbasrc)
{
    GST_DEBUG_OBJECT(vimbasrc, "Applying trigger settings");

    VmbError_t result;
    GEnumValue *enum_entry;

    // TODO: Should  the function start by disabling triggering for all TriggerSelectors to make sure only one is
    // enabled after the function is done?

    // TriggerSelector
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERSELECTOR_VALUES),
                                  vimbasrc->properties.triggerselector);
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"TriggerSelector\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbasrc->camera.handle, "TriggerSelector", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_ERROR_OBJECT(vimbasrc,
                         "Failed to set \"TriggerSelector\" to %s. Return code was: %s",
                         enum_entry->value_nick,
                         ErrorCodeToMessage(result));
        if (result == VmbErrorInvalidValue)
        {
            LogAvailableEnumEntries(vimbasrc, "TriggerSelector");
        }
    }

    // TriggerActivation
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERACTIVATION_VALUES),
                                  vimbasrc->properties.triggeractivation);
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"TriggerActivation\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbasrc->camera.handle, "TriggerActivation", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_ERROR_OBJECT(vimbasrc,
                         "Failed to set \"TriggerActivation\" to %s. Return code was: %s",
                         enum_entry->value_nick,
                         ErrorCodeToMessage(result));
        if (result == VmbErrorInvalidValue)
        {
            LogAvailableEnumEntries(vimbasrc, "TriggerActivation");
        }
    }

    // TriggerSource
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERSOURCE_VALUES),
                                  vimbasrc->properties.triggersource);
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"TriggerSource\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbasrc->camera.handle, "TriggerSource", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_ERROR_OBJECT(vimbasrc,
                         "Failed to set \"TriggerSource\" to %s. Return code was: %s",
                         enum_entry->value_nick,
                         ErrorCodeToMessage(result));
        if (result == VmbErrorInvalidValue)
        {
            LogAvailableEnumEntries(vimbasrc, "TriggerSource");
        }
    }

    // TriggerMode
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERMODE_VALUES),
                                  vimbasrc->properties.triggermode);
    GST_DEBUG_OBJECT(vimbasrc, "Setting \"TriggerMode\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbasrc->camera.handle, "TriggerMode", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbasrc, "Setting was changed successfully");
    }
    else
    {
        GST_ERROR_OBJECT(vimbasrc,
                         "Failed to set \"TriggerMode\" to %s. Return code was: %s",
                         enum_entry->value_nick,
                         ErrorCodeToMessage(result));
    }

    return result;
}

/**
 * @brief Gets the PayloadSize from the connected camera, allocates and announces frame buffers for capturing
 *
 * @param vimbasrc Provides the camera handle used for the Vimba calls and holds the frame buffers
 * @return VmbError_t Return status indicating errors if they occurred
 */
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
            result = VmbFrameAnnounce(vimbasrc->camera.handle,
                                      &vimbasrc->frame_buffers[i],
                                      (VmbUint32_t)sizeof(VmbFrame_t));
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

/**
 * @brief Revokes frame buffers, frees their memory and overwrites old pointers with 0
 *
 * @param vimbasrc Provides the camera handle used for the Vimba calls and the frame buffers
 */
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

/**
 * @brief Starts the capture engine, queues Vimba frames and runs the AcquisitionStart command feature. Frame buffers
 * must be allocated before running this function.
 *
 * @param vimbasrc Provides the camera handle used for the Vimba calls and access to the queued frame buffers
 * @return VmbError_t Return status indicating errors if they occurred
 */
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
            vimbasrc->camera.is_acquiring = true;
        }
    }
    return result;
}

/**
 * @brief Runs the AcquisitionStop command feature, stops the capture engine and flushes the capture queue
 *
 * @param vimbasrc Provides the camera handle which is used for the Vimba function calls
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t stop_image_acquisition(GstVimbaSrc *vimbasrc)
{
    // Stop Acquisition
    GST_DEBUG_OBJECT(vimbasrc, "Running \"AcquisitionStop\" feature");
    VmbError_t result = VmbFeatureCommandRun(vimbasrc->camera.handle, "AcquisitionStop");
    vimbasrc->camera.is_acquiring = false;

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

/**
 * @brief Get the Vimba pixel formats the camera supports and create a mapping of them to compatible GStreamer formats
 * (stored in vimbasrc->camera.supported_formats)
 *
 * @param vimbasrc provides the camera handle and holds the generated mapping
 */
void map_supported_pixel_formats(GstVimbaSrc *vimbasrc)
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
    free((void *)supported_formats);
}

void LogAvailableEnumEntries(GstVimbaSrc *vimbasrc, const char *feat_name)
{
    VmbUint32_t trigger_source_count;
    VmbFeatureEnumRangeQuery(
        vimbasrc->camera.handle,
        feat_name,
        NULL,
        0,
        &trigger_source_count);

    const char **trigger_source_values = malloc(trigger_source_count * sizeof(char *));
    VmbFeatureEnumRangeQuery(
        vimbasrc->camera.handle,
        feat_name,
        trigger_source_values,
        trigger_source_count,
        NULL);

    VmbBool_t is_available;
    GST_ERROR_OBJECT(vimbasrc, "The following values for the \"%s\" feature are available", feat_name);
    for (unsigned int i = 0; i < trigger_source_count; i++)
    {
        VmbFeatureEnumIsAvailable(vimbasrc->camera.handle,
                                  feat_name,
                                  trigger_source_values[i],
                                  &is_available);
        if (is_available)
        {
            GST_ERROR_OBJECT(vimbasrc, "    %s", trigger_source_values[i]);
        }
    }

    free((void *)trigger_source_values);
}
