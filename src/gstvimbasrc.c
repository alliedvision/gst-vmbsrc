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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include "gstvimbasrc.h"

GST_DEBUG_CATEGORY_STATIC(gst_vimba_src_debug_category);
#define GST_CAT_DEFAULT gst_vimba_src_debug_category

/* prototypes */

static void gst_vimba_src_set_property(GObject *object,
                                       guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_vimba_src_get_property(GObject *object,
                                       guint property_id, GValue *value, GParamSpec *pspec);
static void gst_vimba_src_dispose(GObject *object);
static void gst_vimba_src_finalize(GObject *object);

static GstCaps *gst_vimba_src_get_caps(GstBaseSrc *src, GstCaps *filter);
static gboolean gst_vimba_src_set_caps(GstBaseSrc *src, GstCaps *caps);
static gboolean gst_vimba_src_start(GstBaseSrc *src);
static gboolean gst_vimba_src_stop(GstBaseSrc *src);

static GstFlowReturn gst_vimba_src_create(GstPushSrc *src, GstBuffer **buf);

enum
{
    PROP_0
};

/* pad templates */

static GstStaticPadTemplate gst_vimba_src_src_template =
    GST_STATIC_PAD_TEMPLATE("src",
                            GST_PAD_SRC,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS("application/unknown"));

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstVimbaSrc, gst_vimba_src, GST_TYPE_PUSH_SRC,
                        GST_DEBUG_CATEGORY_INIT(gst_vimba_src_debug_category, "vimbasrc", 0,
                                                "debug category for vimbasrc element"));

static void
gst_vimba_src_class_init(GstVimbaSrcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS(klass);
    GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

    /* Setting up pads and setting metadata should be moved to
      base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
                                              &gst_vimba_src_src_template);

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          DESCRIPTION, "Generic", DESCRIPTION,
                                          "Allied Vision Technologies GmbH");

    gobject_class->set_property = gst_vimba_src_set_property;
    gobject_class->get_property = gst_vimba_src_get_property;
    gobject_class->dispose = gst_vimba_src_dispose;
    gobject_class->finalize = gst_vimba_src_finalize;
    base_src_class->get_caps = GST_DEBUG_FUNCPTR(gst_vimba_src_get_caps);
    base_src_class->set_caps = GST_DEBUG_FUNCPTR(gst_vimba_src_set_caps);
    base_src_class->start = GST_DEBUG_FUNCPTR(gst_vimba_src_start);
    base_src_class->stop = GST_DEBUG_FUNCPTR(gst_vimba_src_stop);
    push_src_class->create = GST_DEBUG_FUNCPTR(gst_vimba_src_create);
}

static void
gst_vimba_src_init(GstVimbaSrc *vimbasrc)
{
}

void gst_vimba_src_set_property(GObject *object, guint property_id,
                                const GValue *value, GParamSpec *pspec)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(object);

    GST_DEBUG_OBJECT(vimbasrc, "set_property");

    switch (property_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_vimba_src_get_property(GObject *object, guint property_id,
                                GValue *value, GParamSpec *pspec)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(object);

    GST_DEBUG_OBJECT(vimbasrc, "get_property");

    switch (property_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_vimba_src_dispose(GObject *object)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(object);

    GST_DEBUG_OBJECT(vimbasrc, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_vimba_src_parent_class)->dispose(object);
}

void gst_vimba_src_finalize(GObject *object)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(object);

    GST_DEBUG_OBJECT(vimbasrc, "finalize");

    /* clean up object here */

    G_OBJECT_CLASS(gst_vimba_src_parent_class)->finalize(object);
}

/* get caps from subclass */
static GstCaps *
gst_vimba_src_get_caps(GstBaseSrc *src, GstCaps *filter)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(src);

    GST_DEBUG_OBJECT(vimbasrc, "get_caps");

    return NULL;
}

/* notify the subclass of new caps */
static gboolean
gst_vimba_src_set_caps(GstBaseSrc *src, GstCaps *caps)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(src);

    GST_DEBUG_OBJECT(vimbasrc, "set_caps");

    return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_vimba_src_start(GstBaseSrc *src)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(src);

    GST_DEBUG_OBJECT(vimbasrc, "start");

    return TRUE;
}

static gboolean
gst_vimba_src_stop(GstBaseSrc *src)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(src);

    GST_DEBUG_OBJECT(vimbasrc, "stop");

    return TRUE;
}

/* ask the subclass to create a buffer */
static GstFlowReturn
gst_vimba_src_create(GstPushSrc *src, GstBuffer **buf)
{
    GstVimbaSrc *vimbasrc = GST_VIMBA_SRC(src);

    GST_DEBUG_OBJECT(vimbasrc, "create");

    return GST_FLOW_OK;
}

static gboolean
plugin_init(GstPlugin *plugin)
{

    /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
    return gst_element_register(plugin, "vimbasrc", GST_RANK_NONE,
                                GST_TYPE_VIMBA_SRC);
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
