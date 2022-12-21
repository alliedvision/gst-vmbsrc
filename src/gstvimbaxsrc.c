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
 * SECTION:element-gstvimbaxsrc
 *
 * The vimbaxsrc element provides a way to stream image data into GStreamer pipelines from cameras
 * using the VimbaX API
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v vimbaxsrc camera=<CAMERAID> ! videoconvert ! autovideosink
 * ]|
 * Display a stream from the given camera
 * </refsect2>
 */

#include "gstvimbaxsrc.h"
#include "helpers.h"
#include "vimbax_helpers.h"
#include "pixelformats.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <gst/video/video.h>
#include <gst/video/video-info.h>
#include <glib.h>

#ifdef _WIN32
#include <stdlib.h>
#endif

#include <VmbC/VmbC.h>

// Counter variable to keep track of calls to VmbStartup() and VmbShutdown()
static unsigned int vmb_open_count = 0;
G_LOCK_DEFINE(vmb_open_count);

GST_DEBUG_CATEGORY_STATIC(gst_vimbaxsrc_debug_category);
#define GST_CAT_DEFAULT gst_vimbaxsrc_debug_category

/* prototypes */

static void gst_vimbaxsrc_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_vimbaxsrc_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_vimbaxsrc_finalize(GObject *object);

static GstCaps *gst_vimbaxsrc_get_caps(GstBaseSrc *src, GstCaps *filter);
static gboolean gst_vimbaxsrc_set_caps(GstBaseSrc *src, GstCaps *caps);
static gboolean gst_vimbaxsrc_start(GstBaseSrc *src);
static gboolean gst_vimbaxsrc_stop(GstBaseSrc *src);

static GstFlowReturn gst_vimbaxsrc_create(GstPushSrc *src, GstBuffer **buf);

enum
{
    PROP_0,
    PROP_CAMERA_ID,
    PROP_SETTINGS_FILENAME,
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
    PROP_INCOMPLETE_FRAME_HANDLING,
    PROP_ALLOCATION_MODE
};

/* pad templates */
static GstStaticPadTemplate gst_vimbaxsrc_src_template =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(
                                GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL) ";" GST_BAYER_CAPS_MAKE(GST_BAYER_FORMATS_ALL)));

/* Auto exposure modes */
#define GST_ENUM_EXPOSUREAUTO_MODES (gst_vimbaxsrc_exposureauto_get_type())
static GType gst_vimbaxsrc_exposureauto_get_type(void)
{
    static GType vimbaxsrc_exposureauto_type = 0;
    static const GEnumValue exposureauto_modes[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the VimbaX FeatureEnum */
        {GST_VIMBAXSRC_AUTOFEATURE_OFF, "Exposure duration is usercontrolled using ExposureTime", "Off"},
        {GST_VIMBAXSRC_AUTOFEATURE_ONCE, "Exposure duration is adapted once by the device. Once it has converged, it returns to the Offstate", "Once"},
        {GST_VIMBAXSRC_AUTOFEATURE_CONTINUOUS, "Exposure duration is constantly adapted by the device to maximize the dynamic range", "Continuous"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_exposureauto_type)
    {
        vimbaxsrc_exposureauto_type =
            g_enum_register_static("GstVimbaXSrcExposureAutoModes", exposureauto_modes);
    }
    return vimbaxsrc_exposureauto_type;
}

/* Auto white balance modes */
#define GST_ENUM_BALANCEWHITEAUTO_MODES (gst_vimbaxsrc_balancewhiteauto_get_type())
static GType gst_vimbaxsrc_balancewhiteauto_get_type(void)
{
    static GType vimbaxsrc_balancewhiteauto_type = 0;
    static const GEnumValue balancewhiteauto_modes[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the VimbaX FeatureEnum */
        {GST_VIMBAXSRC_AUTOFEATURE_OFF, "White balancing is user controlled using BalanceRatioSelector and BalanceRatio", "Off"},
        {GST_VIMBAXSRC_AUTOFEATURE_ONCE, "White balancing is automatically adjusted once by the device. Once it has converged, it automatically returns to the Off state", "Once"},
        {GST_VIMBAXSRC_AUTOFEATURE_CONTINUOUS, "White balancing is constantly adjusted by the device", "Continuous"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_balancewhiteauto_type)
    {
        vimbaxsrc_balancewhiteauto_type =
            g_enum_register_static("GstVimbaXSrcBalanceWhiteAutoModes", balancewhiteauto_modes);
    }
    return vimbaxsrc_balancewhiteauto_type;
}

/* TriggerSelector values */
#define GST_ENUM_TRIGGERSELECTOR_VALUES (gst_vimbaxsrc_triggerselector_get_type())
static GType gst_vimbaxsrc_triggerselector_get_type(void)
{
    static GType vimbaxsrc_triggerselector_type = 0;
    static const GEnumValue triggerselector_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the VimbaX FeatureEnum */
        {GST_VIMBAXSRC_TRIGGERSELECTOR_UNCHANGED, "Does not change the currently applied triggerselector value on the device", "UNCHANGED"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_ACQUISITION_START, "Selects a trigger that starts the Acquisition of one or many frames according to AcquisitionMode", "AcquisitionStart"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_ACQUISITION_END, "Selects a trigger that ends the Acquisition of one or many frames according to AcquisitionMode", "AcquisitionEnd"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_ACQUISITION_ACTIVE, "Selects a trigger that controls the duration of the Acquisition of one or many frames. The Acquisition is activated when the trigger signal becomes active and terminated when it goes back to the inactive state", "AcquisitionActive"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_START, "Selects a trigger starting the capture of one frame", "FrameStart"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_END, "Selects a trigger ending the capture of one frame (mainly used in linescanmode)", "FrameEnd"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_ACTIVE, "Selects a trigger controlling the duration of one frame (mainly used in linescanmode)", "FrameActive"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_BURST_START, "Selects a trigger starting the capture of the bursts of frames in an acquisition. AcquisitionBurstFrameCount controls the length of each burst unless a FrameBurstEnd trigger is active. The total number of frames captured is also conditioned by AcquisitionFrameCount if AcquisitionMode is MultiFrame", "FrameBurstStart"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_BURST_END, "Selects a trigger ending the capture of the bursts of frames in an acquisition", "FrameBurstEnd"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_FRAME_BURST_ACTIVE, "Selects a trigger controlling the duration of the capture of the bursts of frames in an acquisition", "FrameBurstActive"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_LINE_START, "Selects a trigger starting the capture of one Line of a Frame (mainly used in linescanmode)", "LineStart"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_EXPOSURE_START, "Selects a trigger controlling the start of the exposure of one Frame (or Line)", "ExposureStart"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_EXPOSURE_END, "Selects a trigger controlling the end of the exposure of one Frame (or Line)", "ExposureEnd"},
        {GST_VIMBAXSRC_TRIGGERSELECTOR_EXPOSURE_ACTIVE, "Selects a trigger controlling the duration of the exposure of one frame (or Line)", "ExposureActive"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_triggerselector_type)
    {
        vimbaxsrc_triggerselector_type =
            g_enum_register_static("GstVimbaXSrcTriggerSelectorValues", triggerselector_values);
    }
    return vimbaxsrc_triggerselector_type;
}

/* TriggerMode values */
#define GST_ENUM_TRIGGERMODE_VALUES (gst_vimbaxsrc_triggermode_get_type())
static GType gst_vimbaxsrc_triggermode_get_type(void)
{
    static GType vimbaxsrc_triggermode_type = 0;
    static const GEnumValue triggermode_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the VimbaX FeatureEnum */
        {GST_VIMBAXSRC_TRIGGERMODE_UNCHANGED, "Does not change the currently applied triggermode value on the device", "UNCHANGED"},
        {GST_VIMBAXSRC_TRIGGERMODE_OFF, "Disables the selected trigger", "Off"},
        {GST_VIMBAXSRC_TRIGGERMODE_ON, "Enable the selected trigger", "On"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_triggermode_type)
    {
        vimbaxsrc_triggermode_type =
            g_enum_register_static("GstVimbaXSrcTriggerModeValues", triggermode_values);
    }
    return vimbaxsrc_triggermode_type;
}

/* TriggerSource values */
#define GST_ENUM_TRIGGERSOURCE_VALUES (gst_vimbaxsrc_triggersource_get_type())
static GType gst_vimbaxsrc_triggersource_get_type(void)
{
    static GType vimbaxsrc_triggersource_type = 0;
    static const GEnumValue triggersource_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the VimbaX FeatureEnum */
        // Commented out trigger sources require more complex setups and should be performed via XML configuration file
        {GST_VIMBAXSRC_TRIGGERSOURCE_UNCHANGED, "Does not change the currently applied triggersource value on the device", "UNCHANGED"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_SOFTWARE, "Specifies that the trigger source will be generated by software using the TriggerSoftware command", "Software"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_LINE0, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line0"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_LINE1, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line1"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_LINE2, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line2"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_LINE3, "Specifies which physical line (or pin) and associated I/O control block to use as external source for the trigger signal", "Line3"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT0, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput0"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT1, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput1"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT2, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput2"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_USER_OUTPUT3, "Specifies which User Output bit signal to use as internal source for the trigger", "UserOutput3"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER0_START, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter0Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER1_START, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter1Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER2_START, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter2Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER3_START, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter3Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER0_END, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter0End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER1_END, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter1End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER2_END, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter2End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_COUNTER3_END, "Specifies which of the Counter signal to use as internal source for the trigger", "Counter3End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER0_START, "Specifies which Timer signal to use as internal source for the trigger", "Timer0Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER1_START, "Specifies which Timer signal to use as internal source for the trigger", "Timer1Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER2_START, "Specifies which Timer signal to use as internal source for the trigger", "Timer2Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER3_START, "Specifies which Timer signal to use as internal source for the trigger", "Timer3Start"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER0_END, "Specifies which Timer signal to use as internal source for the trigger", "Timer0End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER1_END, "Specifies which Timer signal to use as internal source for the trigger", "Timer1End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER2_END, "Specifies which Timer signal to use as internal source for the trigger", "Timer2End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_TIMER3_END, "Specifies which Timer signal to use as internal source for the trigger", "Timer3End"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER0, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder0"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER1, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder1"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER2, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder2"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_ENCODER3, "Specifies which Encoder signal to use as internal source for the trigger", "Encoder3"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK0, "Specifies which Logic Block signal to use as internal source for the trigger", "LogicBlock0"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK1, "Specifies which Logic Block signal to use as internal source for the trigger", "LogicBlock1"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK2, "Specifies which Logic Block signal to use as internal source for the trigger", "LogicBlock2"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LOGIC_BLOCK3, "Specifies which Logic Block signal to use as internal source for the trigger", "LogicBlock3"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_ACTION0, "Specifies which Action command to use as internal source for the trigger", "Action0"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_ACTION1, "Specifies which Action command to use as internal source for the trigger", "Action1"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_ACTION2, "Specifies which Action command to use as internal source for the trigger", "Action2"},
        {GST_VIMBAXSRC_TRIGGERSOURCE_ACTION3, "Specifies which Action command to use as internal source for the trigger", "Action3"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER0, "Specifies which Link Trigger to use as source for the trigger (received from the transport layer)", "LinkTrigger0"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER1, "Specifies which Link Trigger to use as source for the trigger (received from the transport layer)", "LinkTrigger1"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER2, "Specifies which Link Trigger to use as source for the trigger (received from the transport layer)", "LinkTrigger2"},
        // {GST_VIMBAXSRC_TRIGGERSOURCE_LINK_TRIGGER3, "Specifies which Link Trigger to use as source for the trigger (received from the transport layer)", "LinkTrigger3"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_triggersource_type)
    {
        vimbaxsrc_triggersource_type =
            g_enum_register_static("GstVimbaXSrcTriggerSourceValues", triggersource_values);
    }
    return vimbaxsrc_triggersource_type;
}

/* TriggerActivation values */
#define GST_ENUM_TRIGGERACTIVATION_VALUES (gst_vimbaxsrc_triggeractivation_get_type())
static GType gst_vimbaxsrc_triggeractivation_get_type(void)
{
    static GType vimbaxsrc_triggeractivation_type = 0;
    static const GEnumValue triggeractivation_values[] = {
        /* The "nick" (last entry) will be used to pass the setting value on to the VimbaX FeatureEnum */
        {GST_VIMBAXSRC_TRIGGERACTIVATION_UNCHANGED, "Does not change the currently applied triggeractivation value on the device", "UNCHANGED"},
        {GST_VIMBAXSRC_TRIGGERACTIVATION_RISING_EDGE, "Specifies that the trigger is considered valid on the rising edge of the source signal", "RisingEdge"},
        {GST_VIMBAXSRC_TRIGGERACTIVATION_FALLING_EDGE, "Specifies that the trigger is considered valid on the falling edge of the source signal", "FallingEdge"},
        {GST_VIMBAXSRC_TRIGGERACTIVATION_ANY_EDGE, "Specifies that the trigger is considered valid on the falling or rising edge of the source signal", "AnyEdge"},
        {GST_VIMBAXSRC_TRIGGERACTIVATION_LEVEL_HIGH, "Specifies that the trigger is considered valid as long as the level of the source signal is high", "LevelHigh"},
        {GST_VIMBAXSRC_TRIGGERACTIVATION_LEVEL_LOW, "Specifies that the trigger is considered valid as long as the level of the source signal is low", "LevelLow"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_triggeractivation_type)
    {
        vimbaxsrc_triggeractivation_type =
            g_enum_register_static("GstVimbaXSrcTriggerActivationValues", triggeractivation_values);
    }
    return vimbaxsrc_triggeractivation_type;
}

/* IncompleteFrameHandling values */
#define GST_ENUM_INCOMPLETEFRAMEHANDLING_VALUES (gst_vimbaxsrc_incompleteframehandling_get_type())
static GType gst_vimbaxsrc_incompleteframehandling_get_type(void)
{
    static GType vimbaxsrc_incompleteframehandling_type = 0;
    static const GEnumValue incompleteframehandling_values[] = {
        {GST_VIMBAXSRC_INCOMPLETE_FRAME_HANDLING_DROP, "Drop incomplete frames", "Drop"},
        {GST_VIMBAXSRC_INCOMPLETE_FRAME_HANDLING_SUBMIT, "Use incomplete frames and submit them to the next element for processing", "Submit"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_incompleteframehandling_type)
    {
        vimbaxsrc_incompleteframehandling_type =
            g_enum_register_static("GstVimbaXSrcIncompleteFrameHandlingValues", incompleteframehandling_values);
    }
    return vimbaxsrc_incompleteframehandling_type;
}

/* Frame buffer allocation modes */
#define GST_ENUM_ALLOCATIONMODE_VALUES (gst_vimbaxsrc_allocationmode_get_type())
static GType gst_vimbaxsrc_allocationmode_get_type(void)
{
    static GType vimbaxsrc_allocationmode_type = 0;
    static const GEnumValue allocationmode_values[] = {
        {GST_VIMBAXSRC_ALLOCATION_MODE_ANNOUNCE_FRAME, "Allocate buffers in the plugin", "AnnounceFrame"},
        {GST_VIMBAXSRC_ALLOCATION_MODE_ALLOC_AND_ANNOUNCE_FRAME, "Let the transport layer allocate buffers", "AllocAndAnnounceFrame"},
        {0, NULL, NULL}};
    if (!vimbaxsrc_allocationmode_type)
    {
        vimbaxsrc_allocationmode_type =
            g_enum_register_static("GstVimbasrcAllocationModeValues", allocationmode_values);
    }
    return vimbaxsrc_allocationmode_type;
}

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstVimbaXSrc,
                        gst_vimbaxsrc,
                        GST_TYPE_PUSH_SRC,
                        GST_DEBUG_CATEGORY_INIT(gst_vimbaxsrc_debug_category,
                                                "vimbaxsrc",
                                                0,
                                                "debug category for vimbaxsrc element"))

static void gst_vimbaxsrc_class_init(GstVimbaXSrcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
                                              &gst_vimbaxsrc_src_template);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          "VimbaX GStreamer source",
                                          "Generic",
                                          DESCRIPTION,
                                          "Allied Vision Technologies GmbH");

    gobject_class->set_property = gst_vimbaxsrc_set_property;
    gobject_class->get_property = gst_vimbaxsrc_get_property;
    gobject_class->finalize = gst_vimbaxsrc_finalize;
    base_src_class->get_caps = GST_DEBUG_FUNCPTR(gst_vimbaxsrc_get_caps);
    base_src_class->set_caps = GST_DEBUG_FUNCPTR(gst_vimbaxsrc_set_caps);
    base_src_class->start = GST_DEBUG_FUNCPTR(gst_vimbaxsrc_start);
    base_src_class->stop = GST_DEBUG_FUNCPTR(gst_vimbaxsrc_stop);
    push_src_class->create = GST_DEBUG_FUNCPTR(gst_vimbaxsrc_create);

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
        PROP_SETTINGS_FILENAME,
        g_param_spec_string(
            "settingsfile",
            "Camera settings filepath",
            "Path to XML file containing camera settings that should be applied. All settings from this file will be applied before any other property is set. Explicitely set properties will overwrite features set from this file!",
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
            GST_VIMBAXSRC_AUTOFEATURE_OFF,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_BALANCEWHITEAUTO,
        g_param_spec_enum(
            "balancewhiteauto",
            "BalanceWhiteAuto feature setting",
            "Controls the mode for automatic white balancing between the color channels. The white balancing ratios are automatically adjusted",
            GST_ENUM_BALANCEWHITEAUTO_MODES,
            GST_VIMBAXSRC_AUTOFEATURE_OFF,
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
            "Horizontal offset from the origin to the region of interest (in pixels). If -1 is passed the ROI will be centered in the sensor along the horizontal axis.",
            -1,
            G_MAXINT,
            0,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_OFFSETY,
        g_param_spec_int(
            "offsety",
            "OffsetY feature setting",
            "Vertical offset from the origin to the region of interest (in pixels). If -1 is passed the ROI will be centered in the sensor along the vertical axis.",
            -1,
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
            "Selects the type of trigger to configure. Not all cameras support every trigger selector listed below. Check which selectors are supported by the used camera model",
            GST_ENUM_TRIGGERSELECTOR_VALUES,
            GST_VIMBAXSRC_TRIGGERSELECTOR_UNCHANGED,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_TRIGGERMODE,
        g_param_spec_enum(
            "triggermode",
            "TriggerMode feature setting",
            "Controls if the selected trigger is active",
            GST_ENUM_TRIGGERMODE_VALUES,
            GST_VIMBAXSRC_TRIGGERMODE_UNCHANGED,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_TRIGGERSOURCE,
        g_param_spec_enum(
            "triggersource",
            "TriggerSource feature setting",
            "Specifies the internal signal or physical input Line to use as the trigger source. The selected trigger must have its TriggerMode set to On. Not all cameras support every trigger source listed below. Check which sources are supported by the used camera model",
            GST_ENUM_TRIGGERSOURCE_VALUES,
            GST_VIMBAXSRC_TRIGGERSOURCE_UNCHANGED,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_TRIGGERACTIVATION,
        g_param_spec_enum(
            "triggeractivation",
            "TriggerActivation feature setting",
            "Specifies the activation mode of the trigger. Not all cameras support every trigger activation listed below. Check which activations are supported by the used camera model",
            GST_ENUM_TRIGGERACTIVATION_VALUES,
            GST_VIMBAXSRC_TRIGGERACTIVATION_UNCHANGED,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_INCOMPLETE_FRAME_HANDLING,
        g_param_spec_enum(
            "incompleteframehandling",
            "Incomplete frame handling",
            "Determines how the element should handle received frames where data transmission was incomplete. Incomplete frames may contain pixel intensities from old acquisitions or random data",
            GST_ENUM_INCOMPLETEFRAMEHANDLING_VALUES,
            GST_VIMBAXSRC_INCOMPLETE_FRAME_HANDLING_DROP,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    g_object_class_install_property(
        gobject_class,
        PROP_ALLOCATION_MODE,
        g_param_spec_enum(
            "allocationmode",
            "Buffer allocation strategy",
            "Decides if frame buffers should be allocated by the gstreamer element itself or by the transport layer",
            GST_ENUM_ALLOCATIONMODE_VALUES,
            GST_VIMBAXSRC_ALLOCATION_MODE_ANNOUNCE_FRAME,
            G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void gst_vimbaxsrc_init(GstVimbaXSrc *vimbaxsrc)
{
    GST_TRACE_OBJECT(vimbaxsrc, "init");
    GST_INFO_OBJECT(vimbaxsrc, "gst-vimbaxsrc version %s", VERSION);
    VmbError_t result = VmbErrorSuccess;
    // Start the VimbaX API
    G_LOCK(vmb_open_count);
    if (0 == vmb_open_count++)
    {
        result = VmbStartup(NULL);
        GST_DEBUG_OBJECT(vimbaxsrc, "VmbStartup returned: %s", ErrorCodeToMessage(result));
        if (result != VmbErrorSuccess)
        {
            GST_ERROR_OBJECT(vimbaxsrc, "VimbaX initialization failed");
        }
    }
    else
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "VmbStartup was already called. Current open count: %u", vmb_open_count);
    }
    G_UNLOCK(vmb_open_count);

    // Log the used VmbC version
    VmbVersionInfo_t version_info;
    result = VmbVersionQuery(&version_info, sizeof(version_info));
    if (result == VmbErrorSuccess)
    {
        GST_INFO_OBJECT(vimbaxsrc,
                        "Running with VmbC Version %u.%u.%u",
                        version_info.major,
                        version_info.minor,
                        version_info.patch);
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc, "VmbVersionQuery failed with Reason: %s", ErrorCodeToMessage(result));
    }

    // Mark this element as a live source (disable preroll)
    gst_base_src_set_live(GST_BASE_SRC(vimbaxsrc), TRUE);
    gst_base_src_set_format(GST_BASE_SRC(vimbaxsrc), GST_FORMAT_TIME);
    gst_base_src_set_do_timestamp(GST_BASE_SRC(vimbaxsrc), TRUE);

    // Set property helper variables to default values
    GObjectClass *gobject_class = G_OBJECT_GET_CLASS(vimbaxsrc);

    vimbaxsrc->camera.id = g_value_dup_string(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "camera")));
    vimbaxsrc->properties.settings_file_path = g_value_dup_string(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "settingsfile")));
    vimbaxsrc->properties.exposuretime = g_value_get_double(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "exposuretime")));
    vimbaxsrc->properties.exposureauto = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "exposureauto")));
    vimbaxsrc->properties.balancewhiteauto = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "balancewhiteauto")));
    vimbaxsrc->properties.gain = g_value_get_double(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "gain")));
    vimbaxsrc->properties.offsetx = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "offsetx")));
    vimbaxsrc->properties.offsety = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "offsety")));
    vimbaxsrc->properties.width = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "width")));
    vimbaxsrc->properties.height = g_value_get_int(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "height")));
    vimbaxsrc->properties.triggerselector = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggerselector")));
    vimbaxsrc->properties.triggermode = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggermode")));
    vimbaxsrc->properties.triggersource = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggersource")));
    vimbaxsrc->properties.triggeractivation = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "triggeractivation")));
    vimbaxsrc->properties.incomplete_frame_handling = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "incompleteframehandling")));
    vimbaxsrc->properties.allocation_mode = g_value_get_enum(
        g_param_spec_get_default_value(
            g_object_class_find_property(
                gobject_class,
                "allocationmode")));

    gst_video_info_init(&vimbaxsrc->video_info);
}

void gst_vimbaxsrc_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(object);

    GST_DEBUG_OBJECT(vimbaxsrc, "set_property");

    switch (property_id)
    {
    case PROP_CAMERA_ID:
        if (strcmp(vimbaxsrc->camera.id, "") != 0)
        {
            free((void *)vimbaxsrc->camera.id); // Free memory of old entry
        }
        vimbaxsrc->camera.id = g_value_dup_string(value);
        break;
    case PROP_SETTINGS_FILENAME:
        if (strcmp(vimbaxsrc->properties.settings_file_path, "") != 0)
        {
            free((void *)vimbaxsrc->properties.settings_file_path); // Free memory of old entry
        }
        vimbaxsrc->properties.settings_file_path = g_value_dup_string(value);
        break;
    case PROP_EXPOSURETIME:
        vimbaxsrc->properties.exposuretime = g_value_get_double(value);
        break;
    case PROP_EXPOSUREAUTO:
        vimbaxsrc->properties.exposureauto = g_value_get_enum(value);
        break;
    case PROP_BALANCEWHITEAUTO:
        vimbaxsrc->properties.balancewhiteauto = g_value_get_enum(value);
        break;
    case PROP_GAIN:
        vimbaxsrc->properties.gain = g_value_get_double(value);
        break;
    case PROP_OFFSETX:
        vimbaxsrc->properties.offsetx = g_value_get_int(value);
        break;
    case PROP_OFFSETY:
        vimbaxsrc->properties.offsety = g_value_get_int(value);
        break;
    case PROP_WIDTH:
        vimbaxsrc->properties.width = g_value_get_int(value);
        break;
    case PROP_HEIGHT:
        vimbaxsrc->properties.height = g_value_get_int(value);
        break;
    case PROP_TRIGGERSELECTOR:
        vimbaxsrc->properties.triggerselector = g_value_get_enum(value);
        break;
    case PROP_TRIGGERMODE:
        vimbaxsrc->properties.triggermode = g_value_get_enum(value);
        break;
    case PROP_TRIGGERSOURCE:
        vimbaxsrc->properties.triggersource = g_value_get_enum(value);
        break;
    case PROP_TRIGGERACTIVATION:
        vimbaxsrc->properties.triggeractivation = g_value_get_enum(value);
        break;
    case PROP_INCOMPLETE_FRAME_HANDLING:
        vimbaxsrc->properties.incomplete_frame_handling = g_value_get_enum(value);
        break;
    case PROP_ALLOCATION_MODE:
        vimbaxsrc->properties.allocation_mode = g_value_get_enum(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_vimbaxsrc_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(object);

    VmbError_t result;

    const char *vmbfeature_value_char;
    double vmbfeature_value_double;
    VmbInt64_t vmbfeature_value_int64;

    GST_TRACE_OBJECT(vimbaxsrc, "get_property");

    switch (property_id)
    {
    case PROP_CAMERA_ID:
        g_value_set_string(value, vimbaxsrc->camera.id);
        break;
    case PROP_SETTINGS_FILENAME:
        g_value_set_string(value, vimbaxsrc->properties.settings_file_path);
        break;
    case PROP_EXPOSURETIME:
        // TODO: Workaround for cameras with legacy "ExposureTimeAbs" feature should be replaced with a general legacy
        // feature name handling approach: See similar TODO above

        result = VmbFeatureFloatGet(vimbaxsrc->camera.handle, "ExposureTime", &vmbfeature_value_double);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"ExposureTime\": %f",
                             vmbfeature_value_double);
            vimbaxsrc->properties.exposuretime = vmbfeature_value_double;
        }
        else if (result == VmbErrorNotFound)
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to get \"ExposureTime\". Return code was: %s Attempting \"ExposureTimeAbs\"",
                               ErrorCodeToMessage(result));
            result = VmbFeatureFloatGet(vimbaxsrc->camera.handle, "ExposureTimeAbs", &vmbfeature_value_double);
            if (result == VmbErrorSuccess)
            {
                GST_DEBUG_OBJECT(vimbaxsrc,
                                 "Camera returned the following value for \"ExposureTimeAbs\": %f",
                                 vmbfeature_value_double);
                vimbaxsrc->properties.exposuretime = vmbfeature_value_double;
            }
            else
            {
                GST_WARNING_OBJECT(vimbaxsrc,
                                   "Failed to read value of \"ExposureTimeAbs\" from camera. Return code was: %s",
                                   ErrorCodeToMessage(result));
            }
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"ExposureTime\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }

        g_value_set_double(value, vimbaxsrc->properties.exposuretime);
        break;
    case PROP_EXPOSUREAUTO:
        result = VmbFeatureEnumGet(vimbaxsrc->camera.handle, "ExposureAuto", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"ExposureAuto\": %s",
                             vmbfeature_value_char);
            vimbaxsrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                     g_type_class_ref(GST_ENUM_EXPOSUREAUTO_MODES),
                                                     vmbfeature_value_char)
                                                     ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"ExposureAuto\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbaxsrc->properties.exposureauto);
        break;
    case PROP_BALANCEWHITEAUTO:
        result = VmbFeatureEnumGet(vimbaxsrc->camera.handle, "BalanceWhiteAuto", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"BalanceWhiteAuto\": %s",
                             vmbfeature_value_char);
            vimbaxsrc->properties.balancewhiteauto = g_enum_get_value_by_nick(
                                                         g_type_class_ref(GST_ENUM_BALANCEWHITEAUTO_MODES),
                                                         vmbfeature_value_char)
                                                         ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"BalanceWhiteAuto\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbaxsrc->properties.balancewhiteauto);
        break;
    case PROP_GAIN:
        result = VmbFeatureFloatGet(vimbaxsrc->camera.handle, "Gain", &vmbfeature_value_double);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"Gain\": %f",
                             vmbfeature_value_double);
            vimbaxsrc->properties.gain = vmbfeature_value_double;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"Gain\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_double(value, vimbaxsrc->properties.gain);
        break;
    case PROP_OFFSETX:
        result = VmbFeatureIntGet(vimbaxsrc->camera.handle, "OffsetX", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"OffsetX\": %lld",
                             vmbfeature_value_int64);
            vimbaxsrc->properties.offsetx = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Could not read value for \"OffsetX\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbaxsrc->properties.offsetx);
        break;
    case PROP_OFFSETY:
        result = VmbFeatureIntGet(vimbaxsrc->camera.handle, "OffsetY", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"OffsetY\": %lld",
                             vmbfeature_value_int64);
            vimbaxsrc->properties.offsety = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Could not read value for \"OffsetY\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbaxsrc->properties.offsety);
        break;
    case PROP_WIDTH:
        result = VmbFeatureIntGet(vimbaxsrc->camera.handle, "Width", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"Width\": %lld",
                             vmbfeature_value_int64);
            vimbaxsrc->properties.width = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Could not read value for \"Width\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbaxsrc->properties.width);
        break;
    case PROP_HEIGHT:
        result = VmbFeatureIntGet(vimbaxsrc->camera.handle, "Height", &vmbfeature_value_int64);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"Height\": %lld",
                             vmbfeature_value_int64);
            vimbaxsrc->properties.height = (int)vmbfeature_value_int64;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Could not read value for \"Height\". Got return code %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_int(value, vimbaxsrc->properties.height);
        break;
    case PROP_TRIGGERSELECTOR:
        result = VmbFeatureEnumGet(vimbaxsrc->camera.handle, "TriggerSelector", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"TriggerSelector\": %s",
                             vmbfeature_value_char);
            vimbaxsrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                     g_type_class_ref(GST_ENUM_TRIGGERSELECTOR_VALUES),
                                                     vmbfeature_value_char)
                                                     ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"TriggerSelector\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbaxsrc->properties.triggerselector);
        break;
    case PROP_TRIGGERMODE:
        result = VmbFeatureEnumGet(vimbaxsrc->camera.handle, "TriggerMode", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"TriggerMode\": %s",
                             vmbfeature_value_char);
            vimbaxsrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                     g_type_class_ref(GST_ENUM_TRIGGERMODE_VALUES),
                                                     vmbfeature_value_char)
                                                     ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"TriggerMode\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbaxsrc->properties.triggermode);
        break;
    case PROP_TRIGGERSOURCE:
        result = VmbFeatureEnumGet(vimbaxsrc->camera.handle, "TriggerSource", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"TriggerSource\": %s",
                             vmbfeature_value_char);
            vimbaxsrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                     g_type_class_ref(GST_ENUM_TRIGGERSOURCE_VALUES),
                                                     vmbfeature_value_char)
                                                     ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"TriggerSource\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbaxsrc->properties.triggersource);
        break;
    case PROP_TRIGGERACTIVATION:
        result = VmbFeatureEnumGet(vimbaxsrc->camera.handle, "TriggerActivation", &vmbfeature_value_char);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc,
                             "Camera returned the following value for \"TriggerActivation\": %s",
                             vmbfeature_value_char);
            vimbaxsrc->properties.exposureauto = g_enum_get_value_by_nick(
                                                     g_type_class_ref(GST_ENUM_TRIGGERACTIVATION_VALUES),
                                                     vmbfeature_value_char)
                                                     ->value;
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to read value of \"TriggerActivation\" from camera. Return code was: %s",
                               ErrorCodeToMessage(result));
        }
        g_value_set_enum(value, vimbaxsrc->properties.triggeractivation);
        break;
    case PROP_INCOMPLETE_FRAME_HANDLING:
        g_value_set_enum(value, vimbaxsrc->properties.incomplete_frame_handling);
        break;
    case PROP_ALLOCATION_MODE:
        g_value_set_enum(value, vimbaxsrc->properties.incomplete_frame_handling);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_vimbaxsrc_finalize(GObject *object)
{
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(object);

    GST_TRACE_OBJECT(vimbaxsrc, "finalize");

    if (vimbaxsrc->camera.is_connected)
    {
        VmbError_t result = VmbCameraClose(vimbaxsrc->camera.handle);
        if (result == VmbErrorSuccess)
        {
            GST_INFO_OBJECT(vimbaxsrc, "Closed camera %s", vimbaxsrc->camera.id);
        }
        else
        {
            GST_ERROR_OBJECT(vimbaxsrc,
                             "Closing camera %s failed. Got error code: %s",
                             vimbaxsrc->camera.id,
                             ErrorCodeToMessage(result));
        }
        vimbaxsrc->camera.is_connected = false;
    }

    G_LOCK(vmb_open_count);
    if (0 == --vmb_open_count)
    {
        VmbShutdown();
        GST_INFO_OBJECT(vimbaxsrc, "VimbaX API was shut down");
    }
    else
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "VmbShutdown not called. Current open count: %u", vmb_open_count);
    }
    G_UNLOCK(vmb_open_count);

    G_OBJECT_CLASS(gst_vimbaxsrc_parent_class)->finalize(object);
}

/* get caps from subclass */
static GstCaps *gst_vimbaxsrc_get_caps(GstBaseSrc *src, GstCaps *filter)
{
    UNUSED(filter); // enable compilation while treating warning of unused vairable as error
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(src);

    GST_TRACE_OBJECT(vimbaxsrc, "get_caps");

    GstCaps *caps;
    caps = gst_pad_get_pad_template_caps(GST_BASE_SRC_PAD(src));
    caps = gst_caps_make_writable(caps);

    // Query the capabilities from the camera and return sensible values. If no camera is connected the template caps
    // are returned
    if (vimbaxsrc->camera.is_connected)
    {
        VmbInt64_t vmb_width, vmb_height;

        VmbFeatureIntGet(vimbaxsrc->camera.handle, "Width", &vmb_width);
        VmbFeatureIntGet(vimbaxsrc->camera.handle, "Height", &vmb_height);

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
        for (unsigned int i = 0; i < vimbaxsrc->camera.supported_formats_count; i++)
        {
            g_value_set_static_string(&pixel_format, vimbaxsrc->camera.supported_formats[i]->gst_format_name);
            // TODO: Should this perhaps be done via a flag in vimbax_gst_format_matches?
            if (starts_with(vimbaxsrc->camera.supported_formats[i]->vimbax_format_name, "Bayer"))
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

    GST_DEBUG_OBJECT(vimbaxsrc, "returning caps: %s", gst_caps_to_string(caps));

    return caps;
}

/* notify the subclass of new caps */
static gboolean gst_vimbaxsrc_set_caps(GstBaseSrc *src, GstCaps *caps)
{
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(src);

    GST_TRACE_OBJECT(vimbaxsrc, "set_caps");

    GST_DEBUG_OBJECT(vimbaxsrc, "caps requested to be set: %s", gst_caps_to_string(caps));

    // TODO: save to assume that "format" is always exactly one format and not a list? gst_caps_is_fixed might otherwise
    // be a good check and gst_caps_normalize could help make sure of it
    GstStructure *structure;
    structure = gst_caps_get_structure(caps, 0);
    const char *gst_format = gst_structure_get_string(structure, "format");
    GST_DEBUG_OBJECT(vimbaxsrc,
                     "Looking for matching VimbaX pixel format to GSreamer format \"%s\"",
                     gst_format);

    const char *vimbax_format = NULL;
    for (unsigned int i = 0; i < vimbaxsrc->camera.supported_formats_count; i++)
    {
        if (strcmp(gst_format, vimbaxsrc->camera.supported_formats[i]->gst_format_name) == 0)
        {
            vimbax_format = vimbaxsrc->camera.supported_formats[i]->vimbax_format_name;
            GST_DEBUG_OBJECT(vimbaxsrc, "Found matching VimbaX pixel format \"%s\"", vimbax_format);
            break;
        }
    }
    if (vimbax_format == NULL)
    {
        GST_ERROR_OBJECT(vimbaxsrc,
                         "Could not find a matching VimbaX pixel format for GStreamer format \"%s\"",
                         gst_format);
        return FALSE;
    }

    // Apply the requested caps to appropriate camera settings
    VmbError_t result;
    // Changing the pixel format can not be done while images are acquired
    result = stop_image_acquisition(vimbaxsrc);

    result = VmbFeatureEnumSet(vimbaxsrc->camera.handle,
                               "PixelFormat",
                               vimbax_format);
    if (result != VmbErrorSuccess)
    {
        GST_ERROR_OBJECT(vimbaxsrc,
                         "Could not set \"PixelFormat\" to \"%s\". Got return code \"%s\"",
                         vimbax_format,
                         ErrorCodeToMessage(result));
        return FALSE;
    }

    // width and height are always the value that is already written on the camera because get_caps only reports that
    // value. Setting it here is not necessary as the feature values are controlled via properties of the element.

    // Buffer size needs to be increased if the new payload size is greater than the old one because that means the
    // previously allocated buffers are not large enough. We simply check the size of the first buffer because they were
    // all allocated with the same size
    VmbUint32_t new_payload_size;
    result = VmbPayloadSizeGet(vimbaxsrc->camera.handle, &new_payload_size);
    if (vimbaxsrc->frame_buffers[0].bufferSize < new_payload_size || result != VmbErrorSuccess)
    {
        // Also reallocate buffers if PayloadSize could not be read because it might have increased
        GST_DEBUG_OBJECT(vimbaxsrc,
                         "PayloadSize increased. Reallocating frame buffers to ensure enough space");
        revoke_and_free_buffers(vimbaxsrc);
        result = alloc_and_announce_buffers(vimbaxsrc);
    }
    if (result == VmbErrorSuccess)
    {
        result = start_image_acquisition(vimbaxsrc);
    }

    return result == VmbErrorSuccess ? gst_video_info_from_caps(&vimbaxsrc->video_info, caps) : FALSE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean gst_vimbaxsrc_start(GstBaseSrc *src)
{
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(src);

    GST_TRACE_OBJECT(vimbaxsrc, "start");

    // Prepare queue for filled frames from which vimbaxsrc_create can take them
    vimbaxsrc->filled_frame_queue = g_async_queue_new();

    VmbError_t result;

    // TODO: Error handling
    if (!vimbaxsrc->camera.is_connected)
    {
        result = open_camera_connection(vimbaxsrc);
        if (result != VmbErrorSuccess)
        {
            // Can't connect to camera. Abort execution by returning FALSE. This stops the pipeline!
            return FALSE;
        }
    }

    // Load settings from given file if a path was given (settings_file_path is not empty)
    if (strcmp(vimbaxsrc->properties.settings_file_path, "") != 0)
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "\"%s\" was given as settingsfile. Other feature settings passed as element properties will be ignored!",
                           vimbaxsrc->properties.settings_file_path);

        VmbFilePathChar_t *buffer;
#ifdef _WIN32
        size_t num_char = strlen(vimbaxsrc->properties.settings_file_path);
        size_t num_wchar = 0;
        mbstowcs_s(&num_wchar, NULL, 0, vimbaxsrc->properties.settings_file_path, num_char);
        buffer = calloc(num_wchar, sizeof(VmbFilePathChar_t));
        mbstowcs_s(NULL, buffer, num_wchar, vimbaxsrc->properties.settings_file_path, num_char);
#else
        // TODO: THIS NEEDS TO BE TESTED ON A LINUX SYSTEM
        buffer = vimbaxsrc->properties.settings_file_path;
#endif
        VmbFeaturePersistSettings_t settings = {
            .persistType = VmbFeaturePersistStreamable,
            .maxIterations = 1};
        result = VmbSettingsLoad(vimbaxsrc->camera.handle,
                                 buffer,
                                 &settings,
                                 sizeof(settings));
#ifdef _WIN32
        free(buffer);
#endif
        if (result != VmbErrorSuccess)
        {
            GST_ERROR_OBJECT(vimbaxsrc,
                             "Could not load settings from file \"%s\". Got error code %s",
                             vimbaxsrc->properties.settings_file_path,
                             ErrorCodeToMessage(result));
        }
    }
    else
    {
        // If no settings file is given, apply the passed properties as feature settings instead
        GST_DEBUG_OBJECT(vimbaxsrc, "No settings file given. Applying features from element properties instead");
        result = apply_feature_settings(vimbaxsrc);
    }

    result = alloc_and_announce_buffers(vimbaxsrc);
    if (result == VmbErrorSuccess)
    {
        result = start_image_acquisition(vimbaxsrc);
    }

    // Is this necessary?
    if (result == VmbErrorSuccess)
    {
        gst_base_src_start_complete(src, GST_FLOW_OK);
    }
    else
    {
        GST_ERROR_OBJECT(vimbaxsrc, "Could not start acquisition. Experienced error: %s", ErrorCodeToMessage(result));
        gst_base_src_start_complete(src, GST_FLOW_ERROR);
    }

    // TODO: Is this enough error handling?
    return result == VmbErrorSuccess ? TRUE : FALSE;
}

static gboolean gst_vimbaxsrc_stop(GstBaseSrc *src)
{
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(src);

    GST_TRACE_OBJECT(vimbaxsrc, "stop");

    stop_image_acquisition(vimbaxsrc);

    revoke_and_free_buffers(vimbaxsrc);

    // Unref the filled frame queue so it is deleted properly
    g_async_queue_unref(vimbaxsrc->filled_frame_queue);

    return TRUE;
}

/* ask the subclass to create a buffer */
static GstFlowReturn gst_vimbaxsrc_create(GstPushSrc *src, GstBuffer **buf)
{
    GstVimbaXSrc *vimbaxsrc = GST_vimbaxsrc(src);

    GST_TRACE_OBJECT(vimbaxsrc, "create");

    bool submit_frame = false;
    VmbFrame_t *frame;
    do
    {
        // Wait until we can get a filled frame (added to queue in vimbax_frame_callback)
        frame = NULL;
        GstStateChangeReturn ret;
        GstState state;
        do
        {
            // Try to get a filled frame for 10 microseconds
            frame = g_async_queue_timeout_pop(vimbaxsrc->filled_frame_queue, 10);
            // Get the current state of the element. Should return immediately since we are not doing ASYNC state changes
            // but wait at most for 100 nanoseconds
            ret = gst_element_get_state(GST_ELEMENT(vimbaxsrc), &state, NULL, 100); // timeout is given in nanoseconds
            UNUSED(ret);
            if (state != GST_STATE_PLAYING)
            {
                // The src should not create any more data. Stop waiting for frame and do not fill buf
                GST_INFO_OBJECT(vimbaxsrc, "Element state is no longer \"GST_STATE_PLAYING\". Aborting create call.");
                return GST_FLOW_FLUSHING;
            }
        } while (frame == NULL);
        // We got a frame. Check receive status and handle incomplete frames according to
        // vimbaxsrc->properties.incomplete_frame_handling
        if (frame->receiveStatus == VmbFrameStatusIncomplete)
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Received frame with ID \"%llu\" was incomplete", frame->frameID);
            if (vimbaxsrc->properties.incomplete_frame_handling == GST_VIMBAXSRC_INCOMPLETE_FRAME_HANDLING_SUBMIT)
            {
                GST_DEBUG_OBJECT(vimbaxsrc,
                                 "Submitting incomplete frame because \"incompleteframehandling\" requested it");
                submit_frame = true;
            }
            else
            {
                // frame should be dropped -> requeue VimbaX buffer here since image data will not be used
                GST_DEBUG_OBJECT(vimbaxsrc, "Dropping incomplete frame and requeueing buffer to capture queue");
                VmbCaptureFrameQueue(vimbaxsrc->camera.handle, frame, &vimbax_frame_callback);
            }
        }
        else
        {
            GST_TRACE_OBJECT(vimbaxsrc, "frame was complete");
            submit_frame = true;
        }
    } while (!submit_frame);

    // Prepare output buffer that will be filled with frame data
    GstBuffer *buffer = gst_buffer_new_and_alloc(frame->bufferSize);

    // Add a timestamp to the buffer. This is done before copying image data in to keep the
    // timestamp as close to acquisition as possible
    GstClock *clock = gst_element_get_clock(GST_ELEMENT(vimbaxsrc));
    GstClockTime timestamp = GST_CLOCK_TIME_NONE;
    if (clock)
    {
        timestamp = gst_clock_get_time(clock) - gst_element_get_base_time(GST_ELEMENT(vimbaxsrc));
        g_object_unref(clock);
    }
    GST_BUFFER_TIMESTAMP(buffer) = timestamp;
    GST_BUFFER_DURATION(buffer) = GST_CLOCK_TIME_NONE;

    // copy over frame data into the GStreamer buffer
    // TODO: Investigate if we can work without copying to improve performance?
    gst_buffer_fill(
        buffer,
        0,
        frame->buffer,
        frame->bufferSize);

    // requeue frame after we copied the image data for VimbaX to use again
    VmbCaptureFrameQueue(vimbaxsrc->camera.handle, frame, &vimbax_frame_callback);

    // Manually calculate the stride for pixel rows as it might not be identical to GStreamer
    // expectations
    gint stride[GST_VIDEO_MAX_PLANES] = {0};
    gint num_planes = vimbaxsrc->video_info.finfo->n_planes;

    for (gint i = 0; i < num_planes; ++i)
    {
        stride[i] = vimbaxsrc->video_info.width * vimbaxsrc->video_info.finfo->pixel_stride[i];
    }

    gst_buffer_add_video_meta_full(buffer,
                                   GST_VIDEO_FRAME_FLAG_NONE,
                                   vimbaxsrc->video_info.finfo->format,
                                   vimbaxsrc->video_info.width,
                                   vimbaxsrc->video_info.height,
                                   num_planes,
                                   vimbaxsrc->video_info.offset,
                                   stride);

    GST_BUFFER_OFFSET(buffer) = vimbaxsrc->num_frames_pushed;
    GST_BUFFER_OFFSET_END(buffer) = ++(vimbaxsrc->num_frames_pushed);

    // Set filled GstBuffer as output to pass down the pipeline
    *buf = buffer;

    return GST_FLOW_OK;
}

static gboolean plugin_init(GstPlugin *plugin)
{

    /* FIXME Remember to set the rank if it's an element that is meant to be autoplugged by decodebin. */
    return gst_element_register(plugin, "vimbaxsrc", GST_RANK_NONE,
                                GST_TYPE_vimbaxsrc);
}

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  vimbaxsrc,
                  DESCRIPTION,
                  plugin_init,
                  VERSION,
                  "LGPL",
                  PACKAGE,
                  HOMEPAGE_URL)

/**
 * @brief Opens the connection to the camera given by the ID passed as vimbaxsrc property and stores the resulting handle
 *
 * @param vimbaxsrc Provides access to the camera ID and holds the resulting handle
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t open_camera_connection(GstVimbaXSrc *vimbaxsrc)
{
    VmbError_t result = VmbCameraOpen(vimbaxsrc->camera.id, VmbAccessModeFull, &vimbaxsrc->camera.handle);
    if (result == VmbErrorSuccess)
    {
        VmbCameraInfoQuery(vimbaxsrc->camera.id, &vimbaxsrc->camera.info, sizeof(vimbaxsrc->camera.info));
        GST_INFO_OBJECT(vimbaxsrc,
                        "Successfully opened camera %s (model \"%s\", serial \"%s\")",
                        vimbaxsrc->camera.id,
                        vimbaxsrc->camera.info.modelName,
                        vimbaxsrc->camera.info.serialString); // TODO: This seems to show N/A for some cameras (observed with USB)

        // Set the GeV packet size to the highest possible value if a GigE camera is used
        if (VmbErrorSuccess == VmbFeatureCommandRun(vimbaxsrc->camera.info.streamHandles[0], "GVSPAdjustPacketSize"))
        {
            VmbBool_t is_command_done = VmbBoolFalse;
            do
            {
                if (VmbErrorSuccess != VmbFeatureCommandIsDone(vimbaxsrc->camera.info.streamHandles[0],
                                                               "GVSPAdjustPacketSize",
                                                               &is_command_done))
                {
                    break;
                }
            } while (VmbBoolFalse == is_command_done);
        }
        vimbaxsrc->camera.is_connected = true;
        map_supported_pixel_formats(vimbaxsrc);
    }
    else
    {
        GST_ERROR_OBJECT(vimbaxsrc,
                         "Could not open camera %s. Got error code: %s",
                         vimbaxsrc->camera.id,
                         ErrorCodeToMessage(result));
        vimbaxsrc->camera.is_connected = false;
        // TODO: List available cameras in this case?
        // TODO: Can we signal an error to the pipeline to stop immediately?
    }
    vimbaxsrc->camera.is_acquiring = false;
    return result;
}

/**
 * @brief Applies the values defiend in the vimbaxsrc properties to their corresponding camera features
 *
 * @param vimbaxsrc Provides access to the camera handle used for the VmbC calls and holds the desired values for the
 * modified features
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t apply_feature_settings(GstVimbaXSrc *vimbaxsrc)
{
    bool was_acquiring = vimbaxsrc->camera.is_acquiring;
    if (vimbaxsrc->camera.is_acquiring)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Camera was acquiring. Stopping to change feature settings");
        stop_image_acquisition(vimbaxsrc);
    }
    GEnumValue *enum_entry;

    // exposure time
    // TODO: Workaround for cameras with legacy "ExposureTimeAbs" feature should be replaced with a general legacy
    // feature name handling approach: A static table maps each property, e.g. "exposuretime", to a list of (feature
    // name, set function, get function) pairs, e.g. [("ExposureTime", setExposureTime, getExposureTime),
    // ("ExposureTimeAbs", setExposureTimeAbs, getExposureTimeAbs)]. On startup, the feature list of the connected
    // camera obtained from VmbFeaturesList() is used to determine which set/get function to use.

    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"ExposureTime\" to %f", vimbaxsrc->properties.exposuretime);
    VmbError_t result = VmbFeatureFloatSet(vimbaxsrc->camera.handle, "ExposureTime", vimbaxsrc->properties.exposuretime);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else if (result == VmbErrorNotFound)
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"ExposureTime\" to %f. Return code was: %s Attempting \"ExposureTimeAbs\"",
                           vimbaxsrc->properties.exposuretime,
                           ErrorCodeToMessage(result));
        result = VmbFeatureFloatSet(vimbaxsrc->camera.handle, "ExposureTimeAbs", vimbaxsrc->properties.exposuretime);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
        }
        else
        {
            GST_WARNING_OBJECT(vimbaxsrc,
                               "Failed to set \"ExposureTimeAbs\" to %f. Return code was: %s",
                               vimbaxsrc->properties.exposuretime,
                               ErrorCodeToMessage(result));
        }
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"ExposureTime\" to %f. Return code was: %s",
                           vimbaxsrc->properties.exposuretime,
                           ErrorCodeToMessage(result));
    }

    // Exposure Auto
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_EXPOSUREAUTO_MODES), vimbaxsrc->properties.exposureauto);
    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"ExposureAuto\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbaxsrc->camera.handle, "ExposureAuto", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"ExposureAuto\" to %s. Return code was: %s",
                           enum_entry->value_nick,
                           ErrorCodeToMessage(result));
    }

    // Auto whitebalance
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_BALANCEWHITEAUTO_MODES),
                                  vimbaxsrc->properties.balancewhiteauto);
    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"BalanceWhiteAuto\" to %s", enum_entry->value_nick);
    result = VmbFeatureEnumSet(vimbaxsrc->camera.handle, "BalanceWhiteAuto", enum_entry->value_nick);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"BalanceWhiteAuto\" to %s. Return code was: %s",
                           enum_entry->value_nick,
                           ErrorCodeToMessage(result));
    }

    // gain
    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"Gain\" to %f", vimbaxsrc->properties.gain);
    result = VmbFeatureFloatSet(vimbaxsrc->camera.handle, "Gain", vimbaxsrc->properties.gain);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"Gain\" to %f. Return code was: %s",
                           vimbaxsrc->properties.gain,
                           ErrorCodeToMessage(result));
    }

    result = set_roi(vimbaxsrc);

    result = apply_trigger_settings(vimbaxsrc);

    if (was_acquiring)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Camera was acquiring before changing feature settings. Restarting.");
        result = start_image_acquisition(vimbaxsrc);
    }

    return result;
}

/**
 * @brief Helper function to set Width, Height, OffsetX and OffsetY feature in correct order to define the region of
 * interest (ROI) on the sensor.
 *
 * The values for setting the ROI are defined as GStreamer properties of the vimbaxsrc element. If INT_MAX are used for
 * the width/height property (the default value) the full corresponding sensor size for that feature is used.
 *
 * @param vimbaxsrc Provides access to the camera handle used for the VmbC calls and holds the desired values for the
 * modified features
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t set_roi(GstVimbaXSrc *vimbaxsrc)
{
    // TODO: Improve error handling (Perhaps more explicit allowed values are enough?) Early exit on errors?

    // Reset OffsetX and OffsetY to 0 so that full sensor width is usable for width/height
    VmbError_t result;
    GST_DEBUG_OBJECT(vimbaxsrc, "Temporarily resetting \"OffsetX\" and \"OffsetY\" to 0");
    result = VmbFeatureIntSet(vimbaxsrc->camera.handle, "OffsetX", 0);
    if (result != VmbErrorSuccess)
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"OffsetX\" to 0. Return code was: %s",
                           ErrorCodeToMessage(result));
    }
    result = VmbFeatureIntSet(vimbaxsrc->camera.handle, "OffsetY", 0);
    if (result != VmbErrorSuccess)
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"OffsetY\" to 0. Return code was: %s",
                           ErrorCodeToMessage(result));
    }

    VmbInt64_t vmb_width;
    result = VmbFeatureIntRangeQuery(vimbaxsrc->camera.handle, "Width", NULL, &vmb_width);

    // Set Width to full sensor if no explicit width was set
    if (vimbaxsrc->properties.width == INT_MAX)
    {
        GST_DEBUG_OBJECT(vimbaxsrc,
                         "Setting \"Width\" to full width. Got sensor width \"%lld\" (Return Code %s)",
                         vmb_width,
                         ErrorCodeToMessage(result));
        g_object_set(vimbaxsrc, "width", (int)vmb_width, NULL);
    }
    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"Width\" to %d", vimbaxsrc->properties.width);
    result = VmbFeatureIntSet(vimbaxsrc->camera.handle, "Width", vimbaxsrc->properties.width);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"Width\" to value \"%d\". Return code was: %s",
                           vimbaxsrc->properties.width,
                           ErrorCodeToMessage(result));
    }

    VmbInt64_t vmb_height;
    result = VmbFeatureIntRangeQuery(vimbaxsrc->camera.handle, "Height", NULL, &vmb_height);
    // Set Height to full sensor if no explicit height was set
    if (vimbaxsrc->properties.height == INT_MAX)
    {
        GST_DEBUG_OBJECT(vimbaxsrc,
                         "Setting \"Height\" to full height. Got sensor height \"%lld\" (Return Code %s)",
                         vmb_height,
                         ErrorCodeToMessage(result));
        g_object_set(vimbaxsrc, "height", (int)vmb_height, NULL);
    }
    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"Height\" to %d", vimbaxsrc->properties.height);
    result = VmbFeatureIntSet(vimbaxsrc->camera.handle, "Height", vimbaxsrc->properties.height);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"Height\" to value \"%d\". Return code was: %s",
                           vimbaxsrc->properties.height,
                           ErrorCodeToMessage(result));
    }
    // offsetx
    if (vimbaxsrc->properties.offsetx == -1)
    {
        VmbInt64_t vmb_offsetx = (vmb_width - vimbaxsrc->properties.width) >> 1;
        GST_DEBUG_OBJECT(vimbaxsrc, "ROI centering along x-axis requested. Calculated offsetx=%lld",
                         vmb_offsetx);
        g_object_set(vimbaxsrc, "offsetx", (int)vmb_offsetx, NULL);
    }
    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"OffsetX\" to %d", vimbaxsrc->properties.offsetx);
    result = VmbFeatureIntSet(vimbaxsrc->camera.handle, "OffsetX", vimbaxsrc->properties.offsetx);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"OffsetX\" to value \"%d\". Return code was: %s",
                           vimbaxsrc->properties.offsetx,
                           ErrorCodeToMessage(result));
    }

    // offsety
    if (vimbaxsrc->properties.offsety == -1)
    {
        VmbInt64_t vmb_offsety = (vmb_height - vimbaxsrc->properties.height) >> 1;
        GST_DEBUG_OBJECT(vimbaxsrc, "ROI centering along y-axis requested. Calculated offsety=%lld",
                         vmb_offsety);
        g_object_set(vimbaxsrc, "offsety", (int)vmb_offsety, NULL);
    }
    GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"OffsetY\" to %d", vimbaxsrc->properties.offsety);
    result = VmbFeatureIntSet(vimbaxsrc->camera.handle, "OffsetY", vimbaxsrc->properties.offsety);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
    }
    else
    {
        GST_WARNING_OBJECT(vimbaxsrc,
                           "Failed to set \"OffsetY\" to value \"%d\". Return code was: %s",
                           vimbaxsrc->properties.offsety,
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
 * @param vimbaxsrc Provides access to the camera handle used for the VmbC calls and holds the desired values for the
 * modified features
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t apply_trigger_settings(GstVimbaXSrc *vimbaxsrc)
{
    GST_DEBUG_OBJECT(vimbaxsrc, "Applying trigger settings");

    VmbError_t result = VmbErrorSuccess;
    GEnumValue *enum_entry;

    // TODO: Should  the function start by disabling triggering for all TriggerSelectors to make sure only one is
    // enabled after the function is done?

    // TriggerSelector
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERSELECTOR_VALUES),
                                  vimbaxsrc->properties.triggerselector);
    if (enum_entry->value == GST_VIMBAXSRC_TRIGGERSELECTOR_UNCHANGED)
    {
        GST_DEBUG_OBJECT(vimbaxsrc,
                         "\"TriggerSelector\" is set to %s. Not changing camera value", enum_entry->value_nick);
    }
    else
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"TriggerSelector\" to %s", enum_entry->value_nick);
        result = VmbFeatureEnumSet(vimbaxsrc->camera.handle, "TriggerSelector", enum_entry->value_nick);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
        }
        else
        {
            GST_ERROR_OBJECT(vimbaxsrc,
                             "Failed to set \"TriggerSelector\" to %s. Return code was: %s",
                             enum_entry->value_nick,
                             ErrorCodeToMessage(result));
            if (result == VmbErrorInvalidValue)
            {
                log_available_enum_entries(vimbaxsrc, "TriggerSelector");
            }
        }
    }

    // TriggerActivation
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERACTIVATION_VALUES),
                                  vimbaxsrc->properties.triggeractivation);
    if (enum_entry->value == GST_VIMBAXSRC_TRIGGERACTIVATION_UNCHANGED)
    {
        GST_DEBUG_OBJECT(vimbaxsrc,
                         "\"TriggerActivation\" is set to %s. Not changing camera value", enum_entry->value_nick);
    }
    else
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"TriggerActivation\" to %s", enum_entry->value_nick);
        result = VmbFeatureEnumSet(vimbaxsrc->camera.handle, "TriggerActivation", enum_entry->value_nick);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
        }
        else
        {
            GST_ERROR_OBJECT(vimbaxsrc,
                             "Failed to set \"TriggerActivation\" to %s. Return code was: %s",
                             enum_entry->value_nick,
                             ErrorCodeToMessage(result));
            if (result == VmbErrorInvalidValue)
            {
                log_available_enum_entries(vimbaxsrc, "TriggerActivation");
            }
        }
    }

    // TriggerSource
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERSOURCE_VALUES),
                                  vimbaxsrc->properties.triggersource);
    if (enum_entry->value == GST_VIMBAXSRC_TRIGGERSOURCE_UNCHANGED)
    {

        GST_DEBUG_OBJECT(vimbaxsrc,
                         "\"TriggerSource\" is set to %s. Not changing camera value", enum_entry->value_nick);
    }
    else
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"TriggerSource\" to %s", enum_entry->value_nick);
        result = VmbFeatureEnumSet(vimbaxsrc->camera.handle, "TriggerSource", enum_entry->value_nick);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
        }
        else
        {
            GST_ERROR_OBJECT(vimbaxsrc,
                             "Failed to set \"TriggerSource\" to %s. Return code was: %s",
                             enum_entry->value_nick,
                             ErrorCodeToMessage(result));
            if (result == VmbErrorInvalidValue)
            {
                log_available_enum_entries(vimbaxsrc, "TriggerSource");
            }
        }
    }

    // TriggerMode
    enum_entry = g_enum_get_value(g_type_class_ref(GST_ENUM_TRIGGERMODE_VALUES),
                                  vimbaxsrc->properties.triggermode);
    if (enum_entry->value == GST_VIMBAXSRC_TRIGGERMODE_UNCHANGED)
    {
        GST_DEBUG_OBJECT(vimbaxsrc,
                         "\"TriggerMode\" is set to %s. Not changing camera value", enum_entry->value_nick);
    }
    else
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Setting \"TriggerMode\" to %s", enum_entry->value_nick);
        result = VmbFeatureEnumSet(vimbaxsrc->camera.handle, "TriggerMode", enum_entry->value_nick);
        if (result == VmbErrorSuccess)
        {
            GST_DEBUG_OBJECT(vimbaxsrc, "Setting was changed successfully");
        }
        else
        {
            GST_ERROR_OBJECT(vimbaxsrc,
                             "Failed to set \"TriggerMode\" to %s. Return code was: %s",
                             enum_entry->value_nick,
                             ErrorCodeToMessage(result));
        }
    }

    return result;
}

/**
 * @brief Gets the PayloadSize from the connected camera, allocates and announces frame buffers for capturing
 *
 * @param vimbaxsrc Provides the camera handle used for the VmbC calls and holds the frame buffers
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t alloc_and_announce_buffers(GstVimbaXSrc *vimbaxsrc)
{
    VmbUint32_t payload_size;
    VmbError_t result = VmbPayloadSizeGet(vimbaxsrc->camera.handle, &payload_size);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Got \"PayloadSize\" of: %u", payload_size);
        GST_DEBUG_OBJECT(vimbaxsrc, "Allocating and announcing %d VimbaX frames", NUM_FRAME_BUFFERS);
        GEnumValue *allocation_mode = g_enum_get_value(g_type_class_ref(GST_ENUM_ALLOCATIONMODE_VALUES), vimbaxsrc->properties.allocation_mode);
        GST_DEBUG_OBJECT(vimbaxsrc, "Using allocation mode %s", allocation_mode->value_nick);
        for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
        {
            if (vimbaxsrc->properties.allocation_mode == GST_VIMBAXSRC_ALLOCATION_MODE_ANNOUNCE_FRAME)
            {
                // The element is responsible for allocating frame buffers. Some transport layers
                // provide higher performance if specific alignment is observed. Check if this
                // camera has such a requirement. If not this basically becomes a regular allocation
                VmbInt64_t buffer_alignment = 1;
                result = VmbFeatureIntGet(vimbaxsrc->camera.info.streamHandles[0],
                                        "StreamBufferAlignment",
                                        &buffer_alignment);
                // The result is not really important so we do not have to check it. If the camera
                // requires alignment, the call will have succeeded. If alignment does not matter,
                // the call failed but the default value of 1 was not changed
                GST_DEBUG_OBJECT(vimbaxsrc,
                                "Using \"StreamBufferAlignment\" of: %llu (read result was %s)",
                                buffer_alignment,
                                ErrorCodeToMessage(result));
                vimbaxsrc->frame_buffers[i].buffer = VmbAlignedAlloc(buffer_alignment, payload_size);
                if (NULL == vimbaxsrc->frame_buffers[i].buffer)
                {
                    result = VmbErrorResources;
                    break;
                }
            }
            else
            {
                // The transport layer will allocate suitable buffers
                vimbaxsrc->frame_buffers[i].buffer = NULL;
            }

            vimbaxsrc->frame_buffers[i].bufferSize = (VmbUint32_t)payload_size;
            vimbaxsrc->frame_buffers[i].context[0] = vimbaxsrc->filled_frame_queue;

            // Announce Frame
            result = VmbFrameAnnounce(vimbaxsrc->camera.handle,
                                      &vimbaxsrc->frame_buffers[i],
                                      (VmbUint32_t)sizeof(VmbFrame_t));
            if (result != VmbErrorSuccess)
            {
                free(vimbaxsrc->frame_buffers[i].buffer);
                memset(&vimbaxsrc->frame_buffers[i], 0, sizeof(VmbFrame_t));
                break;
            }
        }
    }
    return result;
}

/**
 * @brief Revokes frame buffers, frees their memory and overwrites old pointers with 0
 *
 * @param vimbaxsrc Provides the camera handle used for the VmbC calls and the frame buffers
 */
void revoke_and_free_buffers(GstVimbaXSrc *vimbaxsrc)
{
    for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
    {
        if (NULL != vimbaxsrc->frame_buffers[i].buffer)
        {
            VmbFrameRevoke(vimbaxsrc->camera.handle, &vimbaxsrc->frame_buffers[i]);
            if (vimbaxsrc->properties.allocation_mode == GST_VIMBAXSRC_ALLOCATION_MODE_ANNOUNCE_FRAME)
            {
                // The element allocated the frame buffers, so it must free the memory also
                VmbAlignedFree(vimbaxsrc->frame_buffers[i].buffer);
            }
            memset(&vimbaxsrc->frame_buffers[i], 0, sizeof(VmbFrame_t));
        }
    }
}

/**
 * @brief Starts the capture engine, queues VimbaX frames and runs the AcquisitionStart command feature. Frame buffers
 * must be allocated before running this function.
 *
 * @param vimbaxsrc Provides the camera handle used for the VmbC calls and access to the queued frame buffers
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t start_image_acquisition(GstVimbaXSrc *vimbaxsrc)
{
    // Start Capture Engine
    GST_DEBUG_OBJECT(vimbaxsrc, "Starting the capture engine");
    VmbError_t result = VmbCaptureStart(vimbaxsrc->camera.handle);
    if (result == VmbErrorSuccess)
    {
        GST_DEBUG_OBJECT(vimbaxsrc, "Queueing the VimbaX frames");
        for (int i = 0; i < NUM_FRAME_BUFFERS; i++)
        {
            // Queue Frame
            result = VmbCaptureFrameQueue(vimbaxsrc->camera.handle, &vimbaxsrc->frame_buffers[i], &vimbax_frame_callback);
            if (VmbErrorSuccess != result)
            {
                break;
            }
        }

        if (VmbErrorSuccess == result)
        {
            // Start Acquisition
            GST_DEBUG_OBJECT(vimbaxsrc, "Running \"AcquisitionStart\" feature");
            result = VmbFeatureCommandRun(vimbaxsrc->camera.handle, "AcquisitionStart");
            VmbBool_t acquisition_start_done = VmbBoolFalse;
            do
            {
                if (VmbErrorSuccess != VmbFeatureCommandIsDone(vimbaxsrc->camera.handle,
                                                               "AcquisitionStart",
                                                               &acquisition_start_done))
                {
                    break;
                }
            } while (VmbBoolFalse == acquisition_start_done);
            vimbaxsrc->camera.is_acquiring = true;
        }
    }
    return result;
}

/**
 * @brief Runs the AcquisitionStop command feature, stops the capture engine and flushes the capture queue
 *
 * @param vimbaxsrc Provides the camera handle which is used for the VmbC function calls
 * @return VmbError_t Return status indicating errors if they occurred
 */
VmbError_t stop_image_acquisition(GstVimbaXSrc *vimbaxsrc)
{
    // Stop Acquisition
    GST_DEBUG_OBJECT(vimbaxsrc, "Running \"AcquisitionStop\" feature");
    VmbError_t result = VmbFeatureCommandRun(vimbaxsrc->camera.handle, "AcquisitionStop");
    VmbBool_t acquisition_stop_done = VmbBoolFalse;
    do
    {
        if (VmbErrorSuccess != VmbFeatureCommandIsDone(vimbaxsrc->camera.handle,
                                                       "AcquisitionStop",
                                                       &acquisition_stop_done))
        {
            break;
        }
    } while (VmbBoolFalse == acquisition_stop_done);
    vimbaxsrc->camera.is_acquiring = false;

    // Stop Capture Engine
    GST_DEBUG_OBJECT(vimbaxsrc, "Stopping the capture engine");
    result = VmbCaptureEnd(vimbaxsrc->camera.handle);

    // Flush the capture queue
    GST_DEBUG_OBJECT(vimbaxsrc, "Flushing the capture queue");
    VmbCaptureQueueFlush(vimbaxsrc->camera.handle);

    return result;
}

void VMB_CALL vimbax_frame_callback(const VmbHandle_t camera_handle, const VmbHandle_t stream_handle, VmbFrame_t *frame)
{
    UNUSED(camera_handle); // enable compilation while treating warning of unused vairable as error
    UNUSED(stream_handle);
    GST_TRACE("Got Frame");
    g_async_queue_push(frame->context[0], frame); // context[0] holds vimbaxsrc->filled_frame_queue

    // requeueing the frame is done after it was consumed in vimbaxsrc_create
}

/**
 * @brief Get the VimbaX pixel formats the camera supports and create a mapping of them to compatible GStreamer formats
 * (stored in vimbaxsrc->camera.supported_formats)
 *
 * @param vimbaxsrc provides the camera handle and holds the generated mapping
 */
void map_supported_pixel_formats(GstVimbaXSrc *vimbaxsrc)
{
    // get number of supported formats from the camera
    VmbUint32_t camera_format_count;
    VmbFeatureEnumRangeQuery(
        vimbaxsrc->camera.handle,
        "PixelFormat",
        NULL,
        0,
        &camera_format_count);

    // get the VimbaX format string supported by the camera
    const char **supported_formats = malloc(camera_format_count * sizeof(char *));
    VmbFeatureEnumRangeQuery(
        vimbaxsrc->camera.handle,
        "PixelFormat",
        supported_formats,
        camera_format_count,
        NULL);

    GST_DEBUG_OBJECT(vimbaxsrc, "Camera returned %d supported formats", camera_format_count);
    VmbBool_t is_available;
    for (unsigned int i = 0; i < camera_format_count; i++)
    {
        VmbFeatureEnumIsAvailable(vimbaxsrc->camera.handle, "PixelFormat", supported_formats[i], &is_available);
        if (is_available)
        {
            const VimbaXGstFormatMatch_t *format_map = gst_format_from_vimbax_format(supported_formats[i]);
            if (format_map != NULL)
            {
                GST_DEBUG_OBJECT(vimbaxsrc,
                                 "VimbaX format \"%s\" corresponds to GStreamer format \"%s\"",
                                 supported_formats[i],
                                 format_map->gst_format_name);
                vimbaxsrc->camera.supported_formats[vimbaxsrc->camera.supported_formats_count] = format_map;
                vimbaxsrc->camera.supported_formats_count++;
            }
            else
            {
                GST_DEBUG_OBJECT(vimbaxsrc,
                                 "No corresponding GStreamer format found for VimbaX format \"%s\"",
                                 supported_formats[i]);
            }
        }
        else
        {
            GST_DEBUG_OBJECT(vimbaxsrc, "Reported format \"%s\" is not available", supported_formats[i]);
        }
    }
    free((void *)supported_formats);
}

void log_available_enum_entries(GstVimbaXSrc *vimbaxsrc, const char *feat_name)
{
    VmbUint32_t trigger_source_count;
    VmbFeatureEnumRangeQuery(
        vimbaxsrc->camera.handle,
        feat_name,
        NULL,
        0,
        &trigger_source_count);

    const char **trigger_source_values = malloc(trigger_source_count * sizeof(char *));
    VmbFeatureEnumRangeQuery(
        vimbaxsrc->camera.handle,
        feat_name,
        trigger_source_values,
        trigger_source_count,
        NULL);

    VmbBool_t is_available;
    GST_ERROR_OBJECT(vimbaxsrc, "The following values for the \"%s\" feature are available", feat_name);
    for (unsigned int i = 0; i < trigger_source_count; i++)
    {
        VmbFeatureEnumIsAvailable(vimbaxsrc->camera.handle,
                                  feat_name,
                                  trigger_source_values[i],
                                  &is_available);
        if (is_available)
        {
            GST_ERROR_OBJECT(vimbaxsrc, "    %s", trigger_source_values[i]);
        }
    }

    free((void *)trigger_source_values);
}
