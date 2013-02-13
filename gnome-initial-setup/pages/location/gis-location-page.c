/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Location page {{{1 */

#define PAGE_ID "location"

#include "config.h"
#include "cc-datetime-resources.h"
#include "location-resources.h"
#include "gis-location-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>
#include <string.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/location-entry.h>

#include "cc-timezone-map.h"
#include "timedated.h"

#define DEFAULT_TZ "Europe/London"

G_DEFINE_TYPE (GisLocationPage, gis_location_page, GIS_TYPE_PAGE);

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_LOCATION_PAGE, GisLocationPagePrivate))

struct _GisLocationPagePrivate
{
  CcTimezoneMap *map;
  TzLocation *current_location;
  Timedate1 *dtm;
};

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE (page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
set_timezone_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GisLocationPage *page = user_data;
  GError *error;

  error = NULL;
  if (!timedate1_call_set_timezone_finish (page->priv->dtm,
                                           res,
                                           &error)) {
    /* TODO: display any error in a user friendly way */
    g_warning ("Could not set system timezone: %s", error->message);
    g_error_free (error);
  }
}


static void
queue_set_timezone (GisLocationPage *page)
{
  GisLocationPagePrivate *priv = page->priv;

  /* for now just do it */
  if (priv->current_location) {
    timedate1_call_set_timezone (priv->dtm,
                                 priv->current_location->zone,
                                 TRUE,
                                 NULL,
                                 set_timezone_cb,
                                 page);
  }
}

static void
update_timezone (GisLocationPage *page)
{
  GString *str;
  gchar *location;
  gchar *timezone;
  gchar *c;

  str = g_string_new ("");
  for (c = page->priv->current_location->zone; *c; c++) {
    switch (*c) {
    case '_':
      g_string_append_c (str, ' ');
      break;
    case '/':
      g_string_append (str, " / ");
      break;
    default:
      g_string_append_c (str, *c);
    }
  }

  c = strstr (str->str, " / ");
  location = g_strdup (c + 3);
  timezone = g_strdup (str->str);

  gtk_label_set_label (OBJ(GtkLabel*,"current-location-label"), location);
  gtk_label_set_label (OBJ(GtkLabel*,"current-timezone-label"), timezone);

  g_free (location);
  g_free (timezone);

  g_string_free (str, TRUE);
}

static void
location_changed_cb (CcTimezoneMap   *map,
                     TzLocation      *location,
                     GisLocationPage *page)
{
  g_debug ("location changed to %s/%s", location->country, location->zone);

  page->priv->current_location = location;

  update_timezone (page);

  queue_set_timezone (page);
}

static void
set_location_from_gweather_location (GisLocationPage  *page,
                                     GWeatherLocation *gloc)
{
  GWeatherTimezone *zone = gweather_location_get_timezone (gloc);
  gchar *city = gweather_location_get_city_name (gloc);

  if (zone != NULL) {
    const gchar *name;
    const gchar *id;
    GtkLabel *label;

    label = OBJ(GtkLabel*, "current-timezone-label");

    name = gweather_timezone_get_name (zone);
    id = gweather_timezone_get_tzid (zone);
    if (name == NULL) {
      /* Why does this happen ? */
      name = id;
    }
    gtk_label_set_label (label, name);
    cc_timezone_map_set_timezone (page->priv->map, id);
  }

  if (city != NULL) {
    GtkLabel *label;

    label = OBJ(GtkLabel*, "current-location-label");
    gtk_label_set_label (label, city);
  }

  g_free (city);
}

static void
location_changed (GObject *object, GParamSpec *param, GisLocationPage *page)
{
  GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);
  GWeatherLocation *gloc;

  gloc = gweather_location_entry_get_location (entry);
  if (gloc == NULL)
    return;

  set_location_from_gweather_location (page, gloc);

  gweather_location_unref (gloc);
}

#define WANT_GEOCLUE 0

#if WANT_GEOCLUE
static void
position_callback (GeocluePosition      *pos,
		   GeocluePositionFields fields,
		   int                   timestamp,
		   double                latitude,
		   double                longitude,
		   double                altitude,
		   GeoclueAccuracy      *accuracy,
		   GError               *error,
		   GisLocationPage      *page)
{
  if (error) {
    g_printerr ("Error getting position: %s\n", error->message);
    g_error_free (error);
  } else {
    if (fields & GEOCLUE_POSITION_FIELDS_LATITUDE &&
        fields & GEOCLUE_POSITION_FIELDS_LONGITUDE) {
      GWeatherLocation *city = gweather_location_find_nearest_city (latitude, longitude);
      set_location_from_gweather_location (page, city);
    } else {
      g_print ("Position not available.\n");
    }
  }
}

static void
determine_location (GtkWidget       *widget,
                    GisLocationPage *page)
{
  GeoclueMaster *master;
  GeoclueMasterClient *client;
  GeocluePosition *position = NULL;
  GError *error = NULL;

  master = geoclue_master_get_default ();
  client = geoclue_master_create_client (master, NULL, NULL);
  g_object_unref (master);

  if (!geoclue_master_client_set_requirements (client, 
                                               GEOCLUE_ACCURACY_LEVEL_LOCALITY,
                                               0, TRUE,
                                               GEOCLUE_RESOURCE_ALL,
                                               NULL)){
    g_printerr ("Setting requirements failed");
    goto out;
  }

  position = geoclue_master_client_create_position (client, &error);
  if (position == NULL) {
    g_warning ("Creating GeocluePosition failed: %s", error->message);
    goto out;
  }

  geoclue_position_get_position_async (position,
                                       (GeocluePositionCallback) position_callback,
                                       page);

 out:
  g_clear_error (&error);
  g_object_unref (client);
  g_object_unref (position);
}
#endif

static void
gis_location_page_constructed (GObject *object)
{
  GisLocationPage *page = GIS_LOCATION_PAGE (object);
  GisLocationPagePrivate *priv = page->priv;
  GtkWidget *frame, *map, *entry;
  GWeatherLocation *world;
  GError *error;
  const gchar *timezone;

  G_OBJECT_CLASS (gis_location_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("location-page"));

  frame = WID("location-map-frame");

  error = NULL;
  priv->dtm = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "org.freedesktop.timedate1",
                                                "/org/freedesktop/timedate1",
                                                NULL,
                                                &error);
  if (priv->dtm == NULL) {
    g_error ("Failed to create proxy for timedated: %s", error->message);
    exit (1);
  }

  priv->map = cc_timezone_map_new ();
  map = GTK_WIDGET (priv->map);
  gtk_widget_set_hexpand (map, TRUE);
  gtk_widget_set_vexpand (map, TRUE);
  gtk_widget_set_halign (map, GTK_ALIGN_FILL);
  gtk_widget_set_valign (map, GTK_ALIGN_FILL);
  gtk_widget_show (map);

  gtk_container_add (GTK_CONTAINER (frame), map);

  world = gweather_location_new_world (FALSE);
  entry = gweather_location_entry_new (world);
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry), _("Search for a location"));
  gtk_widget_set_halign (entry, GTK_ALIGN_FILL);
  gtk_widget_show (entry);

  frame = WID("location-page");
#if WANT_GEOCLUE
  gtk_grid_attach (GTK_GRID (frame), entry, 1, 1, 1, 1);
#else
  gtk_grid_attach (GTK_GRID (frame), entry, 0, 1, 2, 1);
#endif

  timezone = timedate1_get_timezone (priv->dtm);

  if (!cc_timezone_map_set_timezone (priv->map, timezone)) {
    g_warning ("Timezone '%s' is unhandled, setting %s as default", timezone, DEFAULT_TZ);
    cc_timezone_map_set_timezone (priv->map, DEFAULT_TZ);
  }
  else {
    g_debug ("System timezone is '%s'", timezone);
  }

  priv->current_location = cc_timezone_map_get_location (priv->map);
  update_timezone (page);

  g_signal_connect (G_OBJECT (entry), "notify::location",
                    G_CALLBACK (location_changed), page);

  g_signal_connect (map, "location-changed",
                    G_CALLBACK (location_changed_cb), page);

#if WANT_GEOCLUE
  g_signal_connect (WID ("location-auto-button"), "clicked",
                    G_CALLBACK (determine_location), page);
#else
  gtk_widget_hide (WID ("location-auto-button"));
#endif

  gis_page_set_title (GIS_PAGE (page), _("Location"));
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_location_page_dispose (GObject *object)
{
  GisLocationPage *page = GIS_LOCATION_PAGE (object);
  GisLocationPagePrivate *priv = page->priv;

  g_clear_object (&priv->dtm);

  G_OBJECT_CLASS (gis_location_page_parent_class)->dispose (object);
}

static void
gis_location_page_class_init (GisLocationPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  object_class->constructed = gis_location_page_constructed;
  object_class->dispose = gis_location_page_dispose;
  
  g_type_class_add_private (object_class, sizeof(GisLocationPagePrivate));
}

static void
gis_location_page_init (GisLocationPage *page)
{
  g_resources_register (location_get_resource ());
  g_resources_register (datetime_get_resource ());
  page->priv = GET_PRIVATE (page);
}

void
gis_prepare_location_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_LOCATION_PAGE,
                                     "driver", driver,
                                     NULL));
}
