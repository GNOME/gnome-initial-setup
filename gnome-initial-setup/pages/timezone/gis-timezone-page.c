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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-wall-clock.h>
#include <gdesktop-enums.h>
#include <geoclue.h>
#include <geocode-glib/geocode-glib.h>

#include <libgweather/gweather.h>

#include "timedated.h"
#include "cc-datetime-resources.h"
#include "timezone-resources.h"

#include "cc-timezone-map.h"
#include "gis-bubble-widget.h"

#include "gis-page-header.h"
#include "gis-location-entry.h"

#define DEFAULT_TZ "Europe/London"
#define DESKTOP_ID "gnome-datetime-panel"

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

/* FIXME: Drop this when we depend on a version of GeoClue which has
 * https://gitlab.freedesktop.org/geoclue/geoclue/-/merge_requests/73 */
typedef GClueSimple MyGClueSimple;
G_DEFINE_AUTOPTR_CLEANUP_FUNC (MyGClueSimple, g_object_unref)

static void stop_geolocation (GisTimezonePage *page);

struct _GisTimezonePagePrivate
{
  GtkWidget *map;
  GtkWidget *search_entry;
  GtkWidget *search_overlay;

  GCancellable *geoclue_cancellable;
  GClueClient *geoclue_client;
  GClueSimple *geoclue_simple;
  gboolean in_geoclue_callback;
  GWeatherLocation *current_location;
  Timedate1 *dtm;
  GCancellable *dtm_cancellable;

  GnomeWallClock *clock;
  GDesktopClockFormat clock_format;
  gboolean in_search;

  gulong search_entry_text_changed_id;
};
typedef struct _GisTimezonePagePrivate GisTimezonePagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GisTimezonePage, gis_timezone_page, GIS_TYPE_PAGE);

static void
set_timezone_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  Timedate1 *dtm = TIMEDATE1 (source);
  g_autoptr(GError) error = NULL;

  if (!timedate1_call_set_timezone_finish (dtm,
                                           res,
                                           &error)) {
    /* TODO: display any error in a user friendly way */
    g_warning ("Could not set system timezone: %s", error->message);
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
                               priv->dtm_cancellable,
                               set_timezone_cb,
                               page);
}

static void
set_location (GisTimezonePage  *page,
              GWeatherLocation *location)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  g_clear_object (&priv->current_location);

  gtk_widget_set_visible (priv->search_overlay, (location == NULL));
  gis_page_set_complete (GIS_PAGE (page), (location != NULL));

  if (location)
    {
      GTimeZone *zone;
      const char *tzid;

      priv->current_location = g_object_ref (location);

      zone = gweather_location_get_timezone (location);
      tzid = g_time_zone_get_identifier (zone);

      cc_timezone_map_set_timezone (CC_TIMEZONE_MAP (priv->map), tzid);

      /* If this location is manually set, stop waiting for geolocation. */
      if (!priv->in_geoclue_callback)
        stop_geolocation (page);
    }
}

static void
on_location_notify (GClueSimple *simple,
                    GParamSpec  *pspec,
                    gpointer     user_data)
{
  GisTimezonePage *page = user_data;
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GClueLocation *location;
  gdouble latitude, longitude;
  g_autoptr(GWeatherLocation) world = gweather_location_get_world ();
  g_autoptr(GWeatherLocation) glocation = NULL;

  location = gclue_simple_get_location (simple);

  latitude = gclue_location_get_latitude (location);
  longitude = gclue_location_get_longitude (location);

  glocation = gweather_location_find_nearest_city (world, latitude, longitude);
  priv->in_geoclue_callback = TRUE;
  set_location (page, glocation);
  priv->in_geoclue_callback = FALSE;
}

static void
on_geoclue_simple_ready (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GisTimezonePage *page = user_data;
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  g_autoptr(GError) local_error = NULL;
  g_autoptr(MyGClueSimple) geoclue_simple = NULL;

  /* This function may be called in an idle callback once @page has been
   * disposed, if going through cancellation. So don’t dereference @priv or
   * @page until the error has been checked. */
  geoclue_simple = gclue_simple_new_finish (res, &local_error);
  if (local_error != NULL)
    {
      if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_info ("Failed to connect to GeoClue2 service: %s", local_error->message);
      return;
    }

  priv->geoclue_simple = g_steal_pointer (&geoclue_simple);
  priv->geoclue_client = gclue_simple_get_client (priv->geoclue_simple);
  gclue_client_set_distance_threshold (priv->geoclue_client,
                                       GEOCODE_LOCATION_ACCURACY_CITY);

  g_signal_connect (priv->geoclue_simple, "notify::location",
                    G_CALLBACK (on_location_notify), page);

  on_location_notify (priv->geoclue_simple, NULL, page);
}

static void
get_location_from_geoclue_async (GisTimezonePage *page)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  gclue_simple_new (DESKTOP_ID,
                    GCLUE_ACCURACY_LEVEL_CITY,
                    priv->geoclue_cancellable,
                    on_geoclue_simple_ready,
                    page);
}

static void
entry_text_changed (GtkEditable *editable,
                    gpointer     user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  stop_geolocation (GIS_TIMEZONE_PAGE (user_data));
  g_signal_handler_disconnect (priv->search_entry,
                               priv->search_entry_text_changed_id);
  priv->search_entry_text_changed_id = 0;
}

static void
entry_location_changed (GObject *object, GParamSpec *param, GisTimezonePage *page)
{
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GisLocationEntry *entry = GIS_LOCATION_ENTRY (object);
  g_autoptr(GWeatherLocation) location = NULL;

  location = gis_location_entry_get_location (entry);
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

  if (priv->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
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

  gtk_widget_set_visible (priv->search_overlay, (location == NULL));
  gis_page_set_complete (GIS_PAGE (page), (location != NULL));

  if (!priv->in_search)
    gtk_editable_set_text (GTK_EDITABLE (priv->search_entry), "");

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
    {
      gclue_client_call_stop (priv->geoclue_client, NULL, NULL, NULL);
      priv->geoclue_client = NULL;
    }
  g_clear_object (&priv->geoclue_simple);
}

static void
gis_timezone_page_root (GtkWidget *widget)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (widget);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);

  GTK_WIDGET_CLASS (gis_timezone_page_parent_class)->root (widget);

 if (priv->geoclue_cancellable == NULL)
   {
     priv->geoclue_cancellable = g_cancellable_new ();
     get_location_from_geoclue_async (page);
   }
}

static void
gis_timezone_page_constructed (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GError *error;
  GSettings *settings;

  G_OBJECT_CLASS (gis_timezone_page_parent_class)->constructed (object);

  priv->dtm_cancellable = g_cancellable_new ();

  error = NULL;
  priv->dtm = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "org.freedesktop.timedate1",
                                                "/org/freedesktop/timedate1",
                                                priv->dtm_cancellable,
                                                &error);
  if (priv->dtm == NULL) {
    g_error ("Failed to create proxy for timedated: %s", error->message);
    exit (1);
  }

  priv->clock = g_object_new (GNOME_TYPE_WALL_CLOCK, NULL);
  g_signal_connect (priv->clock, "notify::clock", G_CALLBACK (on_clock_changed), page);

  settings = g_settings_new (CLOCK_SCHEMA);
  priv->clock_format = g_settings_get_enum (settings, CLOCK_FORMAT_KEY);
  g_object_unref (settings);

  set_location (page, NULL);

  priv->search_entry_text_changed_id =
      g_signal_connect (priv->search_entry, "changed",
                        G_CALLBACK (entry_text_changed), page);
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

  if (priv->dtm_cancellable != NULL)
    {
      g_cancellable_cancel (priv->dtm_cancellable);
      g_clear_object (&priv->dtm_cancellable);
    }

  g_clear_object (&priv->dtm);
  g_clear_object (&priv->clock);

  G_OBJECT_CLASS (gis_timezone_page_parent_class)->dispose (object);
}

static void
gis_timezone_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Time Zone"));
}

static gboolean
gis_timezone_page_apply (GisPage      *page,
                         GCancellable *cancellable)
{
  /* Once the user accepts the location, it would be unkind to change it if
   * GeoClue suddenly tells us we're somewhere else.
   */
  stop_geolocation (GIS_TIMEZONE_PAGE (page));

  return FALSE;
}

static void
gis_timezone_page_class_init (GisTimezonePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-timezone-page.ui");

  gtk_widget_class_bind_template_child_private (widget_class, GisTimezonePage, map);
  gtk_widget_class_bind_template_child_private (widget_class, GisTimezonePage, search_entry);
  gtk_widget_class_bind_template_child_private (widget_class, GisTimezonePage, search_overlay);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_timezone_page_locale_changed;
  page_class->apply = gis_timezone_page_apply;
  object_class->constructed = gis_timezone_page_constructed;
  object_class->dispose = gis_timezone_page_dispose;
  widget_class->root = gis_timezone_page_root;
}

static void
gis_timezone_page_init (GisTimezonePage *page)
{
  g_type_ensure (CC_TYPE_TIMEZONE_MAP);
  g_type_ensure (GIS_TYPE_BUBBLE_WIDGET);
  g_type_ensure (GIS_TYPE_PAGE_HEADER);
  g_type_ensure (GIS_TYPE_LOCATION_ENTRY);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_timezone_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_TIMEZONE_PAGE,
                       "driver", driver,
                       NULL);
}
