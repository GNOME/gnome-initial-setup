/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2013 Red Hat
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
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

#include "config.h"

#include "gis-bubble-widget.h"

struct _GisBubbleWidgetPrivate
{
  GtkWidget *icon;
  GtkWidget *label;
};
typedef struct _GisBubbleWidgetPrivate GisBubbleWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisBubbleWidget, gis_bubble_widget, GTK_TYPE_BIN);

enum {
  PROP_0,
  PROP_LABEL,
  PROP_ICON_NAME,
  PROP_LAST,
};

static GParamSpec *obj_props[PROP_LAST];

static void
gis_bubble_widget_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GisBubbleWidget *widget = GIS_BUBBLE_WIDGET (object);
  GisBubbleWidgetPrivate *priv = gis_bubble_widget_get_instance_private (widget);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (priv->label)));
      break;
    case PROP_ICON_NAME:
      {
        const char *icon_name;
        gtk_image_get_icon_name (GTK_IMAGE (priv->icon), &icon_name, NULL);
        g_value_set_string (value, icon_name);
        break;
      }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gis_bubble_widget_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GisBubbleWidget *widget = GIS_BUBBLE_WIDGET (object);
  GisBubbleWidgetPrivate *priv = gis_bubble_widget_get_instance_private (widget);

  switch (prop_id)
    {
    case PROP_LABEL:
      gtk_label_set_label (GTK_LABEL (priv->label), g_value_get_string (value));
      break;
    case PROP_ICON_NAME:
      g_object_set (GTK_IMAGE (priv->icon),
                    "icon-name", g_value_get_string (value),
                    NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
add_style_from_resource (const char *resource)
{
  GtkCssProvider *provider;
  GFile *file;
  char *uri;

  provider = gtk_css_provider_new ();

  uri = g_strconcat ("resource://", resource, NULL);
  file = g_file_new_for_uri (uri);

  if (!gtk_css_provider_load_from_file (provider, file, NULL))
    goto out;

  gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);

 out:
  g_object_unref (file);
  g_free (uri);
}

static void
gis_bubble_widget_class_init (GisBubbleWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-bubble-widget.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisBubbleWidget, icon);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisBubbleWidget, label);

  object_class->set_property = gis_bubble_widget_set_property;
  object_class->get_property = gis_bubble_widget_get_property;

  obj_props[PROP_LABEL] = g_param_spec_string ("label", "", "", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  obj_props[PROP_ICON_NAME] = g_param_spec_string ("icon-name", "", "", NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, PROP_LAST, obj_props);

  add_style_from_resource ("/org/gnome/initial-setup/gis-bubble-widget.css");
}

static void
gis_bubble_widget_init (GisBubbleWidget *widget)
{
  gtk_widget_init_template (GTK_WIDGET (widget));
}
