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

/* Timezone page {{{1 */

#define PAGE_ID "timezone"

#include "config.h"
#include "cc-datetime-resources.h"
#include "timezone-resources.h"
#include "gis-timezone-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>
#include <string.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/gweather.h>

#include "cc-timezone-map.h"
#include "timedated.h"

#define DEFAULT_TZ "Europe/London"

struct _GisTimezonePagePrivate
{
  CcTimezoneMap *map;
  GWeatherLocation *auto_location;
  GWeatherLocation *current_location;
  Timedate1 *dtm;
};
typedef struct _GisTimezonePagePrivate GisTimezonePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisTimezonePage, gis_timezone_page, GIS_TYPE_PAGE);

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE (page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static void
set_timezone_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GisTimezonePage *page = user_data;
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GError *error;

  error = NULL;
  if (!timedate1_call_set_timezone_finish (priv->dtm,
                                           res,
                                           &error)) {
    /* TODO: display any error in a user friendly way */
    g_warning ("Could not set system timezone: %s", error->message);
    g_error_free (error);
  }
}


static void
queue_set_timezone (GisTimezonePage *page,
                    const char      *tzid)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  /* for now just do it */
  timedate1_call_set_timezone (priv->dtm,
                               tzid,
                               TRUE,
                               NULL,
                               set_timezone_cb,
                               page);
}

static void
set_location (GisTimezonePage  *page,
              GWeatherLocation *location)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  if (priv->current_location)
    gweather_location_unref (priv->current_location);

  cc_timezone_map_set_location (priv->map, location);

  if (location)
    {
      GWeatherTimezone *zone;
      const char *tzid;

      priv->current_location = gweather_location_ref (location);

      zone = gweather_location_get_timezone (location);
      tzid = gweather_timezone_get_tzid (zone);

      queue_set_timezone (page, tzid);
    }
}

static char *
get_location_name (GWeatherLocation *location)
{
  GWeatherTimezone *zone = gweather_location_get_timezone (location);

  /* XXX -- do something smarter eventually */
  return g_strdup_printf ("%s (GMT%+g)",
                          gweather_location_get_name (location),
                          gweather_timezone_get_offset (zone) / 60.0);
}

static void
set_auto_location (GisTimezonePage  *page,
                   GWeatherLocation *location)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  if (priv->auto_location)
    gweather_location_unref (priv->auto_location);

  if (location)
    {
      char *tzname, *markup;
      priv->auto_location = gweather_location_ref (location);

      tzname = get_location_name (location);
      markup = g_strdup_printf (_("We think that your time zone is <b>%s</b>. Press Next to continue"
                                  " or search for a city to manually set the time zone."),
                                tzname);
      gtk_label_set_markup (GTK_LABEL (WID ("timezone-auto-result")), markup);
      g_free (tzname);
      g_free (markup);
   }
  else
    {
      priv->auto_location = NULL;

      /* We have no automatic location; transition to search automatically */
      gtk_widget_hide (WID ("timezone-search-button"));
      gtk_widget_hide (WID ("timezone-auto-result"));
    }

  gtk_widget_show (WID ("timezone-stack"));
}

static void
get_location_from_geoclue (GisTimezonePage *page)
{
  GDBusProxy *manager = NULL, *client = NULL, *location = NULL;
  GVariant *value;
  const char *object_path;
  double latitude, longitude;
  GWeatherLocation *glocation = NULL;

  manager = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           NULL,
                                           "org.freedesktop.GeoClue2",
                                           "/org/freedesktop/GeoClue2/Manager",
                                           "org.freedesktop.GeoClue2.Manager",
                                           NULL, NULL);
  if (!manager)
    goto out;

  value = g_dbus_proxy_call_sync (manager, "GetClient", NULL,
                                  G_DBUS_CALL_FLAGS_NONE, -1,
                                  NULL, NULL);
  if (!value)
    goto out;

  g_variant_get_child (value, 0, "&o", &object_path);

  client = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.GeoClue2",
                                          object_path,
                                          "org.freedesktop.GeoClue2.Client",
                                          NULL, NULL);
  g_variant_unref (value);

  if (!client)
    goto out;

  value = g_dbus_proxy_get_cached_property (client, "Location");
  object_path = g_variant_get_string (value, NULL);

  location = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                            G_DBUS_PROXY_FLAGS_NONE,
                                            NULL,
                                            "org.freedesktop.GeoClue2",
                                            object_path,
                                            "org.freedesktop.GeoClue2.Location",
                                            NULL, NULL);
  g_variant_unref (value);

  if (!location)
    goto out;

  value = g_dbus_proxy_get_cached_property (location, "Latitude");

  /* this happens under some circumstances, iunno why. needs zeenix */
  if (!value)
    goto out;

  latitude = g_variant_get_double (value);
  g_variant_unref (value);
  value = g_dbus_proxy_get_cached_property (location, "Longitude");
  longitude = g_variant_get_double (value);
  g_variant_unref (value);

  glocation = gweather_location_find_nearest_city (NULL, latitude, longitude);

 out:
  set_auto_location (page, glocation);
  set_location (page, glocation);
  if (glocation)
    gweather_location_unref (glocation);

  if (manager)
    g_object_unref (manager);
  if (client)
    g_object_unref (client);
  if (location)
    g_object_unref (location);
}

static void
entry_location_changed (GObject *object, GParamSpec *param, GisTimezonePage *page)
{
  GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);
  GWeatherLocation *location;

  location = gweather_location_entry_get_location (entry);
  if (!location)
    return;

  set_location (page, location);
}

static void
entry_mapped (GtkWidget *widget,
              gpointer   user_data)
{
  gtk_widget_grab_focus (widget);
}

static void
visible_child_changed (GObject *object, GParamSpec *param, GisTimezonePage *page)
{
  /* xxx -- text bubble */
  /*
  GtkWidget *child = gtk_stack_get_visible_child (GTK_STACK (WID ("timezone-stack")));

  if (child == WID ("timezone-search")) {
  }
  */
}

static void
search_button_toggled (GtkToggleButton *button,
                       GisTimezonePage *page)
{
  gboolean want_search = gtk_toggle_button_get_active (button);

  gtk_stack_set_visible_child_name (GTK_STACK (WID ("timezone-stack")),
                                    want_search ? "search" : "status");
}

static void
gis_timezone_page_constructed (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GtkWidget *frame, *map;
  GError *error;

  G_OBJECT_CLASS (gis_timezone_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("timezone-page"));

  frame = WID("timezone-map-frame");

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

  frame = WID("timezone-page");

  get_location_from_geoclue (page);

  g_signal_connect (WID ("timezone-search"), "notify::location",
                    G_CALLBACK (entry_location_changed), page);
  g_signal_connect (WID ("timezone-search"), "map",
                    G_CALLBACK (entry_mapped), page);
  g_signal_connect (WID ("timezone-stack"), "notify::visible-child",
                    G_CALLBACK (visible_child_changed), page);
  g_signal_connect (WID ("timezone-search-button"), "toggled",
                    G_CALLBACK (search_button_toggled), page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_timezone_page_dispose (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  g_clear_object (&priv->dtm);

  G_OBJECT_CLASS (gis_timezone_page_parent_class)->dispose (object);
}

static void
gis_timezone_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Time Zone"));
}

static void
gis_timezone_page_class_init (GisTimezonePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_timezone_page_locale_changed;
  object_class->constructed = gis_timezone_page_constructed;
  object_class->dispose = gis_timezone_page_dispose;
}

static void
gis_timezone_page_init (GisTimezonePage *page)
{
  g_resources_register (timezone_get_resource ());
  g_resources_register (datetime_get_resource ());
}

void
gis_prepare_timezone_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_TIMEZONE_PAGE,
                                     "driver", driver,
                                     NULL));
}
