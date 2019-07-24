/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/* -*- encoding: utf8 -*- */
/*
 * Copyright (C) 2019 Purism SPC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Adrien Plazas <kekun.plazas@laposte.net>
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gis-page-header.h"

enum {
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_ICON_NAME,
  PROP_SHOW_ICON,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

struct _GisPageHeaderPrivate
{
  GtkWidget *box;
  GtkWidget *icon;
  GtkWidget *subtitle;
  GtkWidget *title;
};
typedef struct _GisPageHeaderPrivate GisPageHeaderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisPageHeader, gis_page_header, GTK_TYPE_BOX)

static gboolean
is_valid_string (const gchar *s)
{
  return s != NULL && g_strcmp0 (s, "") != 0;
}

static void
update_box_visibility (GisPageHeader *header)
{
  GisPageHeaderPrivate *priv = gis_page_header_get_instance_private (header);

  gtk_widget_set_visible (priv->box, gtk_widget_get_visible (priv->subtitle) ||
                                     gtk_widget_get_visible (priv->title));
}

static void
gis_page_header_init (GisPageHeader *header)
{
  GisPageHeaderPrivate *priv = gis_page_header_get_instance_private (header);

  gtk_widget_init_template (GTK_WIDGET (header));

  g_signal_connect_swapped (priv->subtitle, "notify::visible",
                            G_CALLBACK(update_box_visibility), header);
  g_signal_connect_swapped (priv->title, "notify::visible",
                            G_CALLBACK(update_box_visibility), header);
}

static void
gis_page_header_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GisPageHeader *header = GIS_PAGE_HEADER (object);
  GisPageHeaderPrivate *priv = gis_page_header_get_instance_private (header);
  const gchar *icon_name = NULL;

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->title)));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->subtitle)));
      break;

    case PROP_ICON_NAME:
      g_object_get (priv->icon, "icon-name", &icon_name, NULL);
      g_value_set_string (value, icon_name);
      break;

    case PROP_SHOW_ICON:
      g_value_set_boolean (value, gtk_widget_get_visible (priv->icon));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_page_header_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GisPageHeader *header = GIS_PAGE_HEADER (object);
  GisPageHeaderPrivate *priv = gis_page_header_get_instance_private (header);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_label (GTK_LABEL (priv->title), g_value_get_string (value));
      gtk_widget_set_visible (priv->title, is_valid_string (g_value_get_string (value)));
      break;

    case PROP_SUBTITLE:
      gtk_label_set_label (GTK_LABEL (priv->subtitle), g_value_get_string (value));
      gtk_widget_set_visible (priv->subtitle, is_valid_string (g_value_get_string (value)));
      break;

    case PROP_ICON_NAME:
      g_object_set (priv->icon, "icon-name", g_value_get_string (value), NULL);
      break;

    case PROP_SHOW_ICON:
      gtk_widget_set_visible (priv->icon, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_page_header_class_init (GisPageHeaderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-page-header.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPageHeader, box);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPageHeader, icon);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPageHeader, subtitle);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisPageHeader, title);

  gobject_class->get_property = gis_page_header_get_property;
  gobject_class->set_property = gis_page_header_set_property;

  obj_props[PROP_TITLE] =
    g_param_spec_string ("title",
                         "", "",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_SUBTITLE] =
    g_param_spec_string ("subtitle",
                         "", "",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_ICON_NAME] =
    g_param_spec_string ("icon-name",
                         "", "",
                         NULL,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  obj_props[PROP_SHOW_ICON] =
    g_param_spec_boolean ("show-icon",
                          "", "",
                          FALSE,
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_props);
}
