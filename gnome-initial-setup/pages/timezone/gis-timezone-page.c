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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Timezone page {{{1 */

#define PAGE_ID "timezone"

#include "config.h"
#include "gis-timezone-page.h"

#include <glib/gi18n.h>
#include <gio/gio.h>

#include <stdlib.h>
#include <string.h>
#include <langinfo.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-wall-clock.h>
#include <gdesktop-enums.h>

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/gweather.h>

#include "geoclue.h"
#include "timedated.h"
#include "cc-datetime-resources.h"
#include "timezone-resources.h"

#include "cc-timezone-map.h"
#include "gis-bubble-widget.h"

#define DEFAULT_TZ "Europe/London"
#define DESKTOP_ID "gnome-datetime-panel"

/* Defines from geoclue private header src/public-api/gclue-enums.h */
#define GCLUE_ACCURACY_LEVEL_CITY 4

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

struct _GisTimezonePagePrivate
{
  GtkWidget *map;
  GtkWidget *search_entry;
  GtkWidget *search_overlay;

  GCancellable *geoclue_cancellable;
  GeoclueClient *geoclue_client;
  GeoclueManager *geoclue_manager;
  GWeatherLocation *current_location;
  Timedate1 *dtm;

  GnomeWallClock *clock;
  GDesktopClockFormat clock_format;
  gboolean ampm_available;
  gboolean in_search;
};
typedef struct _GisTimezonePagePrivate GisTimezonePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisTimezonePage, gis_timezone_page, GIS_TYPE_PAGE);

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

  g_clear_pointer (&priv->current_location, gweather_location_unref);

  gtk_widget_set_visible (priv->search_overlay, (location == NULL));
  gis_page_set_complete (GIS_PAGE (page), (location != NULL));

  if (location)
    {
      GWeatherTimezone *zone;
      const char *tzid;

      priv->current_location = gweather_location_ref (location);

      zone = gweather_location_get_timezone (location);
      tzid = gweather_timezone_get_tzid (zone);

      cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (priv->map), tzid);
    }
}

static char *
get_location_name (GWeatherLocation *location)
{
  GDateTime *datetime;
  GTimeZone *timezone;
  char *country;
  char *ret;
  const char *country_code;
  const char *timezone_name;
  const char *timezone_id;

  timezone_id = gweather_timezone_get_tzid (gweather_location_get_timezone (location));
  timezone = g_time_zone_new (timezone_id);
  datetime = g_date_time_new_now (timezone);
  timezone_name = g_date_time_get_timezone_abbreviation (datetime);

  country_code = gweather_location_get_country (location);
  country = gnome_get_country_from_code (country_code, NULL);

  ret = g_strdup_printf ("<b>%s (%s, %s)</b>",
                         timezone_name,
                         gweather_location_get_city_name (location),
                         country);

  g_time_zone_unref (timezone);
  g_date_time_unref (datetime);
  g_free (country);

  return ret;
}

static void
on_location_proxy_ready (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GisTimezonePage *page = user_data;
  GeoclueLocation *location;
  gdouble latitude, longitude;
  GError *error = NULL;
  GWeatherLocation *glocation = NULL;

  location = geoclue_location_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      g_critical ("Failed to connect to GeoClue2 service: %s", error->message);
      g_error_free (error);
      return;
    }

  latitude = geoclue_location_get_latitude (location);
  longitude = geoclue_location_get_longitude (location);

  glocation = gweather_location_find_nearest_city (NULL, latitude, longitude);

  set_location (page, glocation);

  g_object_unref (location);
  gweather_location_unref (glocation);
}

static void
on_location_updated (GDBusProxy *client,
                     gchar      *location_path_old,
                     gchar      *location_path_new,
                     gpointer    user_data)
{
  GisTimezonePage *page = user_data;
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  geoclue_location_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                      G_DBUS_PROXY_FLAGS_NONE,
                                      "org.freedesktop.GeoClue2",
                                      location_path_new,
                                      priv->geoclue_cancellable,
                                      on_location_proxy_ready,
                                      page);
}

static void
on_start_ready (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
  GError *error = NULL;

  if (!geoclue_client_call_start_finish (GEOCLUE_CLIENT (source_object),
                                         res,
                                         &error))
    {
      g_critical ("Failed to start GeoClue2 client: %s", error->message);
      g_error_free (error);
      return;
    }
}

static void
on_client_proxy_ready (GObject      *source_object,
                       GAsyncResult *res,
                       gpointer      user_data)
{
  GisTimezonePage *page = user_data;
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GError *error = NULL;

  priv->geoclue_client = geoclue_client_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      g_critical ("Failed to connect to GeoClue2 service: %s", error->message);
      g_error_free (error);
      return;
    }

  geoclue_client_set_desktop_id (priv->geoclue_client, DESKTOP_ID);
  geoclue_client_set_requested_accuracy_level (priv->geoclue_client,
                                               GCLUE_ACCURACY_LEVEL_CITY);

  g_signal_connect (priv->geoclue_client, "location-updated",
                    G_CALLBACK (on_location_updated), page);

  geoclue_client_call_start (priv->geoclue_client,
                             priv->geoclue_cancellable,
                             on_start_ready,
                             page);
}

static void
on_get_client_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  GisTimezonePage *page = user_data;
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  gchar *client_path;
  GError *error = NULL;

  if (!geoclue_manager_call_get_client_finish (GEOCLUE_MANAGER (source_object),
                                               &client_path,
                                               res,
                                               &error))
    {
      g_critical ("Failed to connect to GeoClue2 service: %s", error->message);
      g_error_free (error);
      return;
    }

  geoclue_client_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                    G_DBUS_PROXY_FLAGS_NONE,
                                    "org.freedesktop.GeoClue2",
                                    client_path,
                                    priv->geoclue_cancellable,
                                    on_client_proxy_ready,
                                    page);
}

static void
on_manager_proxy_ready (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{

  GisTimezonePage *page = user_data;
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GError *error = NULL;

  priv->geoclue_manager = geoclue_manager_proxy_new_for_bus_finish (res, &error);
  if (error != NULL)
    {
      g_critical ("Failed to connect to GeoClue2 service: %s", error->message);
      g_error_free (error);
      return;
    }

  geoclue_manager_call_get_client (priv->geoclue_manager,
                                   priv->geoclue_cancellable,
                                   on_get_client_ready,
                                   page);
}

static void
get_location_from_geoclue_async (GisTimezonePage *page)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  geoclue_manager_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                     G_DBUS_PROXY_FLAGS_NONE,
                                     "org.freedesktop.GeoClue2",
                                     "/org/freedesktop/GeoClue2/Manager",
                                     priv->geoclue_cancellable,
                                     on_manager_proxy_ready,
                                     page);
}

static void
entry_location_changed (GObject *object, GParamSpec *param, GisTimezonePage *page)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GWeatherLocationEntry *entry = GWEATHER_LOCATION_ENTRY (object);
  GWeatherLocation *location;

  location = gweather_location_entry_get_location (entry);
  if (!location)
    return;

  priv->in_search = TRUE;
  set_location (page, location);
  priv->in_search = FALSE;
}

#define GETTEXT_PACKAGE_TIMEZONES "gnome-control-center-2.0-timezones"

static char *
translated_city_name (TzLocation *loc)
{
  char *country;
  char *name;
  char *zone_translated;
  char **split_translated;
  gint length;

  /* Load the translation for it */
  zone_translated = g_strdup (dgettext (GETTEXT_PACKAGE_TIMEZONES, loc->zone));
  g_strdelimit (zone_translated, "_", ' ');
  split_translated = g_regex_split_simple ("[\\x{2044}\\x{2215}\\x{29f8}\\x{ff0f}/]",
                                           zone_translated,
                                           0, 0);
  g_free (zone_translated);

  length = g_strv_length (split_translated);

  country = gnome_get_country_from_code (loc->country, NULL);
  /* Translators: "city, country" */
  name = g_strdup_printf (C_("timezone loc", "%s, %s"),
                          split_translated[length-1],
                          country);
  g_free (country);
  g_strfreev (split_translated);

  return name;
}

static void
update_timezone (GisTimezonePage *page, TzLocation *location)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  char *tz_desc;
  char *bubble_text;
  char *city_country;
  char *utc_label;
  char *time_label;
  GTimeZone *zone;
  GDateTime *date;
  gboolean use_ampm;

  if (priv->clock_format == G_DESKTOP_CLOCK_FORMAT_12H && priv->ampm_available)
    use_ampm = TRUE;
  else
    use_ampm = FALSE;

  zone = g_time_zone_new (location->zone);
  date = g_date_time_new_now (zone);
  g_time_zone_unref (zone);

  /* Update the text bubble in the timezone map */
  city_country = translated_city_name (location);

 /* Translators: UTC here means the Coordinated Universal Time.
  * %:::z will be replaced by the offset from UTC e.g. UTC+02
  */
  utc_label = g_date_time_format (date, _("UTC%:::z"));

  if (use_ampm)
    /* Translators: This is the time format used in 12-hour mode. */
    time_label = g_date_time_format (date, _("%l:%M %p"));
  else
    /* Translators: This is the time format used in 24-hour mode. */
    time_label = g_date_time_format (date, _("%R"));

  /* Translators: "timezone (utc shift)" */
  tz_desc = g_strdup_printf (C_("timezone map", "%s (%s)"),
                             g_date_time_get_timezone_abbreviation (date),
                             utc_label);
  bubble_text = g_strdup_printf ("<b>%s</b>\n"
                                 "<small>%s</small>\n"
                                 "<b>%s</b>",
                                 tz_desc,
                                 city_country,
                                 time_label);
  cc_timezone_map_set_bubble_text (CC_TIMEZONE_MAP (priv->map), bubble_text);

  g_free (tz_desc);
  g_free (city_country);
  g_free (utc_label);
  g_free (time_label);
  g_free (bubble_text);

  g_date_time_unref (date);
}

static void
map_location_changed (CcTimezoneMap   *map,
                      TzLocation      *location,
                      GisTimezonePage *page)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  if (!priv->in_search)
    gtk_entry_set_text (GTK_ENTRY (priv->search_entry), "");

  update_timezone (page, location);
  queue_set_timezone (page, location->zone);
}

static void
on_clock_changed (GnomeWallClock  *clock,
                  GParamSpec      *pspec,
                  GisTimezonePage *page)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  TzLocation *location;

  if (!gtk_widget_get_mapped (priv->map))
    return;

  if (gtk_widget_is_visible (priv->search_overlay))
    return;

  location = cc_timezone_map_get_location (CC_TIMEZONE_MAP (priv->map));
  if (location)
    update_timezone (page, location);
}

static void
entry_mapped (GtkWidget *widget,
              gpointer   user_data)
{
  gtk_widget_grab_focus (widget);
}

static void
stop_geolocation (GisTimezonePage *page)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  if (priv->geoclue_cancellable)
    {
      g_cancellable_cancel (priv->geoclue_cancellable);
      g_clear_object (&priv->geoclue_cancellable);
    }

  if (priv->geoclue_client)
    geoclue_client_call_stop (priv->geoclue_client, NULL, NULL, NULL);
  g_clear_object (&priv->geoclue_client);
  g_clear_object (&priv->geoclue_manager);
}

static void
gis_timezone_page_constructed (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GError *error;
  const char *ampm;
  GSettings *settings;

  G_OBJECT_CLASS (gis_timezone_page_parent_class)->constructed (object);

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

  priv->clock = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
  g_signal_connect (priv->clock, "notify::clock", G_CALLBACK (on_clock_changed), page);

  ampm = nl_langinfo (AM_STR);
  if (ampm == NULL || ampm[0] == '\0')
    priv->ampm_available = FALSE;
  else
    priv->ampm_available = TRUE;

  settings = g_settings_new (CLOCK_SCHEMA);
  priv->clock_format = g_settings_get_enum (settings, CLOCK_FORMAT_KEY);
  g_object_unref (settings);

  priv->geoclue_cancellable = g_cancellable_new ();

  set_location (page, NULL);
  get_location_from_geoclue_async (page);

  g_signal_connect (priv->search_entry, "notify::location",
                    G_CALLBACK (entry_location_changed), page);
  g_signal_connect (priv->search_entry, "map",
                    G_CALLBACK (entry_mapped), page);
  g_signal_connect (priv->map, "location-changed",
                    G_CALLBACK (map_location_changed), page);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_timezone_page_dispose (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  stop_geolocation (page);

  g_clear_object (&priv->dtm);
  g_clear_object (&priv->clock);

  G_OBJECT_CLASS (gis_timezone_page_parent_class)->dispose (object);
}

static void
gis_timezone_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Time Zone"));
}

static void
gis_timezone_page_shown (GisPage *page)
{
  GisTimezonePage *tz_page = GIS_TIMEZONE_PAGE (page);

  /* Stop timezone geolocation if it hasn't finished by the time we get here */
  stop_geolocation (tz_page);
}

static void
gis_timezone_page_class_init (GisTimezonePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-timezone-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisTimezonePage, map);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisTimezonePage, search_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisTimezonePage, search_overlay);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_timezone_page_locale_changed;
  page_class->shown = gis_timezone_page_shown;
  object_class->constructed = gis_timezone_page_constructed;
  object_class->dispose = gis_timezone_page_dispose;
}

static void
gis_timezone_page_init (GisTimezonePage *page)
{
  g_resources_register (timezone_get_resource ());
  g_resources_register (datetime_get_resource ());
  g_type_ensure (CC_TYPE_TIMEZONE_MAP);
  g_type_ensure (GIS_TYPE_BUBBLE_WIDGET);

  gtk_widget_init_template (GTK_WIDGET (page));
}

void
gis_prepare_timezone_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_TIMEZONE_PAGE,
                                     "driver", driver,
                                     NULL));
}
