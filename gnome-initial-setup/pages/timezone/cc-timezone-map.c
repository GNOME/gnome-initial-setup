/*
 * Copyright (C) 2010 Intel, Inc
 *
 * Portions from Ubiquity, Copyright (C) 2009 Canonical Ltd.
 * Written by Evan Dandrea <evand@ubuntu.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */

#include "cc-timezone-map.h"
#include <math.h>
#include <string.h>

G_DEFINE_TYPE (CcTimezoneMap, cc_timezone_map, GTK_TYPE_WIDGET)

#define TIMEZONE_MAP_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_TIMEZONE_MAP, CcTimezoneMapPrivate))

#define PIN_HOT_POINT_X 8
#define PIN_HOT_POINT_Y 15

#define DATETIME_RESOURCE_PATH "/org/gnome/control-center/datetime"

struct _CcTimezoneMapPrivate
{
  GdkPixbuf *orig_background;
  GdkPixbuf *orig_background_dim;

  GdkPixbuf *background;

  GWeatherLocation *location;
};

static void
cc_timezone_map_dispose (GObject *object)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (object)->priv;

  g_clear_object (&priv->orig_background);
  g_clear_object (&priv->orig_background_dim);
  g_clear_object (&priv->background);

  G_OBJECT_CLASS (cc_timezone_map_parent_class)->dispose (object);
}

/* GtkWidget functions */
static void
cc_timezone_map_get_preferred_width (GtkWidget *widget,
                                     gint      *minimum,
                                     gint      *natural)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (widget)->priv;
  gint size;

  size = gdk_pixbuf_get_width (priv->orig_background);

  if (minimum != NULL)
    *minimum = size;
  if (natural != NULL)
    *natural = size;
}

static void
cc_timezone_map_get_preferred_height (GtkWidget *widget,
                                      gint      *minimum,
                                      gint      *natural)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (widget)->priv;
  gint size;

  size = gdk_pixbuf_get_height (priv->orig_background);

  if (minimum != NULL)
    *minimum = size;
  if (natural != NULL)
    *natural = size;
}

static void
cc_timezone_map_size_allocate (GtkWidget     *widget,
                               GtkAllocation *allocation)
{
  CcTimezoneMapPrivate *priv = CC_TIMEZONE_MAP (widget)->priv;
  GdkPixbuf *pixbuf;

  if (priv->background)
    g_object_unref (priv->background);

  if (!gtk_widget_is_sensitive (widget))
    pixbuf = priv->orig_background_dim;
  else
    pixbuf = priv->orig_background;

  priv->background = gdk_pixbuf_scale_simple (pixbuf,
                                              allocation->width,
                                              allocation->height,
                                              GDK_INTERP_BILINEAR);

  GTK_WIDGET_CLASS (cc_timezone_map_parent_class)->size_allocate (widget,
                                                                  allocation);
}

static gdouble
convert_longitude_to_x (gdouble longitude, gint map_width)
{
  const gdouble xdeg_offset = -6;
  gdouble x;

  x = (map_width * (180.0 + longitude) / 360.0)
    + (map_width * xdeg_offset / 180.0);

  return x;
}

static gdouble
radians (gdouble degrees)
{
  return (degrees / 360.0) * G_PI * 2;
}

static gdouble
convert_latitude_to_y (gdouble latitude, gdouble map_height)
{
  gdouble bottom_lat = -59;
  gdouble top_lat = 81;
  gdouble top_per, y, full_range, top_offset, map_range;

  top_per = top_lat / 180.0;
  y = 1.25 * log (tan (G_PI_4 + 0.4 * radians (latitude)));
  full_range = 4.6068250867599998;
  top_offset = full_range * top_per;
  map_range = fabs (1.25 * log (tan (G_PI_4 + 0.4 * radians (bottom_lat))) - top_offset);
  y = fabs (y - top_offset);
  y = y / map_range;
  y = y * map_height;
  return y;
}

static void
draw_hilight (CcTimezoneMap *map,
              cairo_t       *cr)
{
  GtkWidget *widget = GTK_WIDGET (map);
  CcTimezoneMapPrivate *priv = map->priv;
  const char *fmt;
  GWeatherTimezone *zone;
  double selected_offset;
  GdkPixbuf *hilight, *orig_hilight;
  GtkAllocation alloc;
  char *file;
  GError *err = NULL;

  if (!priv->location)
    return;

  gtk_widget_get_allocation (widget, &alloc);

  /* paint hilight */
  if (gtk_widget_is_sensitive (widget))
    fmt = DATETIME_RESOURCE_PATH "/timezone_%g.png";
  else
    fmt = DATETIME_RESOURCE_PATH "/timezone_%g_dim.png";

  zone = gweather_location_get_timezone (priv->location);

  /* XXX: Do we need to do anything for DST? I don't think so... */
  selected_offset = gweather_timezone_get_offset (zone) / 60.0;

  file = g_strdup_printf (fmt, selected_offset);
  orig_hilight = gdk_pixbuf_new_from_resource (file, &err);
  g_free (file);
  file = NULL;

  hilight = gdk_pixbuf_scale_simple (orig_hilight, alloc.width, alloc.height, GDK_INTERP_BILINEAR);
  gdk_cairo_set_source_pixbuf (cr, hilight, 0, 0);

  cairo_paint (cr);
  g_object_unref (hilight);
  g_object_unref (orig_hilight);

  g_clear_error (&err);
}

static void
draw_pin (CcTimezoneMap *map,
          cairo_t       *cr)
{
  GtkWidget *widget = GTK_WIDGET (map);
  CcTimezoneMapPrivate *priv = map->priv;
  GdkPixbuf *pin;
  GtkAllocation alloc;
  GError *err = NULL;
  double longitude, latitude;
  double pointx, pointy;

  gtk_widget_get_allocation (widget, &alloc);

  if (!priv->location)
    return;

  if (!gweather_location_has_coords (priv->location))
    return;

  /* load pin icon */
  pin = gdk_pixbuf_new_from_resource (DATETIME_RESOURCE_PATH "/pin.png", &err);

  gweather_location_get_coords (priv->location, &latitude, &longitude);

  pointx = convert_longitude_to_x (longitude, alloc.width);
  pointy = convert_latitude_to_y (latitude, alloc.height);

  pointx = CLAMP (floor (pointx), 0, alloc.width);
  pointy = CLAMP (floor (pointy), 0, alloc.height);

  gdk_cairo_set_source_pixbuf (cr, pin, pointx - PIN_HOT_POINT_X, pointy - PIN_HOT_POINT_Y);
  cairo_paint (cr);

  g_object_unref (pin);
}

static gboolean
cc_timezone_map_draw (GtkWidget *widget,
                      cairo_t   *cr)
{
  CcTimezoneMap *map = CC_TIMEZONE_MAP (widget);
  CcTimezoneMapPrivate *priv = map->priv;

  /* paint background */
  gdk_cairo_set_source_pixbuf (cr, priv->background, 0, 0);
  cairo_paint (cr);

  draw_hilight (map, cr);
  draw_pin (map, cr);

  return TRUE;
}

static void
cc_timezone_map_class_init (CcTimezoneMapClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcTimezoneMapPrivate));

  object_class->dispose = cc_timezone_map_dispose;

  widget_class->get_preferred_width = cc_timezone_map_get_preferred_width;
  widget_class->get_preferred_height = cc_timezone_map_get_preferred_height;
  widget_class->size_allocate = cc_timezone_map_size_allocate;
  widget_class->draw = cc_timezone_map_draw;
}

static void
cc_timezone_map_init (CcTimezoneMap *self)
{
  CcTimezoneMapPrivate *priv;
  GError *err = NULL;

  priv = self->priv = TIMEZONE_MAP_PRIVATE (self);

  priv->orig_background = gdk_pixbuf_new_from_resource (DATETIME_RESOURCE_PATH "/bg.png",
                                                        &err);

  if (!priv->orig_background)
    {
      g_warning ("Could not load background image: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  priv->orig_background_dim = gdk_pixbuf_new_from_resource (DATETIME_RESOURCE_PATH "/bg_dim.png",
                                                            &err);

  if (!priv->orig_background_dim)
    {
      g_warning ("Could not load background image: %s",
                 (err) ? err->message : "Unknown error");
      g_clear_error (&err);
    }

  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);
}

CcTimezoneMap *
cc_timezone_map_new (void)
{
  return g_object_new (CC_TYPE_TIMEZONE_MAP, NULL);
}

void
cc_timezone_map_set_location (CcTimezoneMap    *map,
                              GWeatherLocation *location)
{
  CcTimezoneMapPrivate *priv = map->priv;

  if (priv->location)
    gweather_location_unref (priv->location);

  if (location)
    priv->location = gweather_location_ref (location);
  else
    priv->location = NULL;

  gtk_widget_queue_draw (GTK_WIDGET (map));
}
