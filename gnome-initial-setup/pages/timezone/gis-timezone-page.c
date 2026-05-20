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

#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include <stdlib.h>
#include <string.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <gdesktop-enums.h>
#include <geoclue.h>
#include <geocode-glib/geocode-glib.h>

#include <libgweather/gweather.h>

#include "timedated.h"

#include "gis-page-header.h"

#define DESKTOP_ID "gnome-datetime-panel"

#define CLOCK_SCHEMA "org.gnome.desktop.interface"
#define CLOCK_FORMAT_KEY "clock-format"

#define LOCATION_SCHEMA "org.gnome.system.location"
#define LOCATION_ENABLED_KEY "enabled"

static void stop_geolocation (GisTimezonePage *page);

struct _GisTimezonePage
{
  GisPage parent;

  GtkWidget          *search_entry;
  GtkWidget          *listbox;
  GListStore         *countries;
  GListStore         *cities;
  GtkFilter          *filter;
  GtkFilterListModel *filter_model;
  GtkSliceListModel  *slice_model;
  GWeatherLocation   *selected_country;
  GWeatherLocation   *world;
  gboolean            showing_cities;
  char *casefolded_search_text;

  GCancellable *geoclue_cancellable;
  GClueClient *geoclue_client;
  GClueSimple *geoclue_simple;
  gboolean in_geoclue_callback;
  gboolean is_complete;
  GWeatherLocation *current_location;
  Timedate1 *dtm;
  GCancellable *dtm_cancellable;
  
  GDesktopClockFormat clock_format;

  GSettings *location_settings;
};

static void
search_entry_activated (GtkEditable *editable,
                        gpointer     user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);

  if (page->is_complete)
    gis_assistant_next_page (gis_driver_get_assistant (GIS_PAGE (page)->driver));
}

G_DEFINE_TYPE (GisTimezonePage, gis_timezone_page, GIS_TYPE_PAGE);

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
  /* for now just do it */
  timedate1_call_set_timezone (page->dtm,
                               tzid,
                               TRUE,
                               page->dtm_cancellable,
                               set_timezone_cb,
                               page);
}

static void
set_location (GisTimezonePage  *page,
              GWeatherLocation *location)
{
  GTimeZone *zone;

  g_clear_object (&page->current_location);

  if (!location) {
    page->is_complete = FALSE;
    gis_page_set_complete (GIS_PAGE (page), FALSE);
    return;
  }

  zone = gweather_location_get_timezone (location);
  if (!zone) {
    page->is_complete = FALSE;
    gis_page_set_complete (GIS_PAGE (page), FALSE);
    return;
  }

  page->current_location = g_object_ref (location);
  page->is_complete = TRUE;
  gis_page_set_complete (GIS_PAGE (page), TRUE);

  queue_set_timezone (page, g_time_zone_get_identifier (zone));

  if (!page->in_geoclue_callback)
    stop_geolocation (page);
}

static void
on_location_notify (GClueSimple *simple,
                    GParamSpec  *pspec,
                    gpointer     user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  GClueLocation *location;
  gdouble latitude, longitude;
  g_autoptr(GWeatherLocation) glocation = NULL;

  location = gclue_simple_get_location (simple);

  latitude = gclue_location_get_latitude (location);
  longitude = gclue_location_get_longitude (location);

  glocation = gweather_location_find_nearest_city (page->world, latitude, longitude);
  page->in_geoclue_callback = TRUE;
  set_location (page, glocation);
  page->in_geoclue_callback = FALSE;
}

static void
on_geoclue_simple_ready (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  g_autoptr(GError) local_error = NULL;
  g_autoptr(GClueSimple) geoclue_simple = NULL;

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

  page->geoclue_simple = g_steal_pointer (&geoclue_simple);
  page->geoclue_client = gclue_simple_get_client (page->geoclue_simple);
  gclue_client_set_distance_threshold (page->geoclue_client,
                                       GEOCODE_LOCATION_ACCURACY_CITY);

  g_signal_connect (page->geoclue_simple, "notify::location",
                    G_CALLBACK (on_location_notify), page);

  on_location_notify (page->geoclue_simple, NULL, page);
}

static void
get_location_from_geoclue_async (GisTimezonePage *page)
{
  gclue_simple_new (DESKTOP_ID,
                    GCLUE_ACCURACY_LEVEL_CITY,
                    page->geoclue_cancellable,
                    on_geoclue_simple_ready,
                    page);
}

static char *
get_location_display_name (GWeatherLocation *location)
{
  GWeatherLocation *parent;
  const char *city_name;
  const char *country_name = NULL;
  const char *adm1_name = NULL;

  city_name = gweather_location_get_name (location);
  if (!city_name)
    return NULL;

  for (parent = gweather_location_get_parent (location);
       parent != NULL;
       parent = gweather_location_get_parent (parent)) {
    switch (gweather_location_get_level (parent)) {
    case GWEATHER_LOCATION_COUNTRY:
      country_name = gweather_location_get_name (parent);
      break;
    case GWEATHER_LOCATION_ADM1:
      adm1_name = gweather_location_get_name (parent);
      break;
    default:
      break;
    }
  }

  if (adm1_name && country_name)
    return g_strdup_printf (_("%s, %s, %s"), city_name, adm1_name, country_name);

  if (country_name)
    return g_strdup_printf (_("%s, %s"), city_name, country_name);

  return g_strdup (city_name);
}

static void
fill_countries_model (GListStore       *store,
                      GWeatherLocation *location)
{
  g_autoptr(GWeatherLocation) child = NULL;

  switch (gweather_location_get_level (location)) {
  case GWEATHER_LOCATION_WORLD:
  case GWEATHER_LOCATION_REGION:
    while ((child = gweather_location_next_child (location, child)))
      fill_countries_model (store, child);
    break;

  case GWEATHER_LOCATION_COUNTRY:
    g_list_store_append (store, location);
    break;

  case GWEATHER_LOCATION_ADM1:
  case GWEATHER_LOCATION_CITY:
  case GWEATHER_LOCATION_NAMED_TIMEZONE:
  case GWEATHER_LOCATION_WEATHER_STATION:
    break;

  case GWEATHER_LOCATION_DETACHED:
    g_assert_not_reached ();
  }
}

static void
fill_cities_model (GListStore       *store,
                   GWeatherLocation *location)
{
  g_autoptr(GWeatherLocation) child = NULL;

  switch (gweather_location_get_level (location)) {
  case GWEATHER_LOCATION_WORLD:
  case GWEATHER_LOCATION_REGION:
  case GWEATHER_LOCATION_COUNTRY:
  case GWEATHER_LOCATION_ADM1:
    while ((child = gweather_location_next_child (location, child)))
      fill_cities_model (store, child);
    break;

  case GWEATHER_LOCATION_CITY:
    if (gweather_location_get_timezone (location) == NULL)
      break;

    g_list_store_append (store, location);
    break;

  case GWEATHER_LOCATION_NAMED_TIMEZONE:
  case GWEATHER_LOCATION_WEATHER_STATION:
    break;

  case GWEATHER_LOCATION_DETACHED:
    g_assert_not_reached ();
  }
}

static gboolean
location_is_in_country (GWeatherLocation *location,
                        GWeatherLocation *country)
{
  GWeatherLocation *parent;

  if (!location || !country)
    return FALSE;

  for (parent = gweather_location_get_parent (location);
       parent != NULL;
       parent = gweather_location_get_parent (parent)) {
    if (gweather_location_equal (parent, country))
      return TRUE;
  }

  return FALSE;
}

static gboolean
location_filter_match_cb (gpointer item,
                          gpointer user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  GWeatherLocation *location = GWEATHER_LOCATION (item);
  g_autofree char *name = NULL;
  g_autofree char *normalized_name = NULL;
  g_autofree char *casefolded_name = NULL;

  if (page->showing_cities && page->selected_country != NULL) {
    if (!location_is_in_country (location, page->selected_country))
      return FALSE;
  }

  if (!page->casefolded_search_text || *page->casefolded_search_text == '\0')
    return TRUE;

  if (gweather_location_get_level (location) == GWEATHER_LOCATION_CITY)
    name = get_location_display_name (location);
  else
    name = g_strdup (gweather_location_get_name (location));

  if (!name)
    return FALSE;

  normalized_name = g_utf8_normalize (name, -1, G_NORMALIZE_ALL);
  if (!normalized_name)
    return FALSE;

  casefolded_name = g_utf8_casefold (normalized_name, -1);
  if (!casefolded_name)
    return FALSE;

  return strstr (casefolded_name, page->casefolded_search_text) != NULL;
}

static void
show_countries (GisTimezonePage *page)
{
  page->showing_cities = FALSE;
  g_clear_object (&page->selected_country);

  gtk_slice_list_model_set_offset (page->slice_model, 0);

  gtk_filter_list_model_set_model (page->filter_model,
                                   G_LIST_MODEL (page->countries));

  gtk_slice_list_model_set_size (page->slice_model, G_MAXUINT);

  if (page->filter)
    gtk_filter_changed (page->filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
show_cities_for_country (GisTimezonePage  *page,
                         GWeatherLocation *country)
{
  g_clear_object (&page->selected_country);
  page->selected_country = g_object_ref (country);
  page->showing_cities = TRUE;

  gtk_slice_list_model_set_offset (page->slice_model, 0);
  gtk_slice_list_model_set_size (page->slice_model, 20);

  gtk_filter_list_model_set_model (page->filter_model,
                                   G_LIST_MODEL (page->cities));

  if (page->filter)
    gtk_filter_changed (page->filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
show_all_cities_for_search (GisTimezonePage *page)
{
  g_clear_object (&page->selected_country);
  page->showing_cities = TRUE;

  gtk_slice_list_model_set_offset (page->slice_model, 0);
  gtk_slice_list_model_set_size (page->slice_model, 20);

  gtk_filter_list_model_set_model (page->filter_model,
                                   G_LIST_MODEL (page->cities));

  if (page->filter)
    gtk_filter_changed (page->filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static void
entry_text_changed (GtkEditable *editable,
                    gpointer     user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  const char *text;

  stop_geolocation (page);

  text = gtk_editable_get_text (editable);

  g_clear_pointer (&page->casefolded_search_text, g_free);

  if (text && *text != '\0') {
    g_autofree char *normalized_text = NULL;

    normalized_text = g_utf8_normalize (text, -1, G_NORMALIZE_ALL);
    if (normalized_text)
      page->casefolded_search_text = g_utf8_casefold (normalized_text, -1);
  }

  if (!text || *text == '\0') {
    show_countries (page);
    return;
  }

  if (!page->showing_cities || page->selected_country != NULL) {
    show_all_cities_for_search (page);
    return;
  }

  gtk_slice_list_model_set_offset (page->slice_model, 0);

  if (page->filter)
    gtk_filter_changed (page->filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static char *
format_location_time (GisTimezonePage  *page,
                      GWeatherLocation *location)
{
  GTimeZone *zone;
  g_autoptr(GDateTime) date = NULL;

  zone = gweather_location_get_timezone (location);
  if (!zone)
    return NULL;

  date = g_date_time_new_now (zone);

  if (page->clock_format == G_DESKTOP_CLOCK_FORMAT_12H)
    return g_date_time_format (date, _("%l:%M %p"));

  return g_date_time_format (date, _("%R"));
}

static GtkWidget *
create_location_row_widget (gpointer item,
                            gpointer user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  GWeatherLocation *location = GWEATHER_LOCATION (item);
  GtkWidget *row;
  g_autofree char *display_name = NULL;
  g_autofree char *time = NULL;

  row = adw_action_row_new ();

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);

  g_object_set_data_full (G_OBJECT (row),
                          "location",
                          g_object_ref (location),
                          g_object_unref);

  switch (gweather_location_get_level (location)) {
  case GWEATHER_LOCATION_COUNTRY:
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                   gweather_location_get_name (location));
    break;

  case GWEATHER_LOCATION_CITY:
    display_name = get_location_display_name (location);
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), display_name);

    time = format_location_time (page, location);
    if (time)
      adw_action_row_set_subtitle (ADW_ACTION_ROW (row), time);

    if (page->current_location &&
        gweather_location_equal (location, page->current_location)) {
      GtkWidget *check;

      check = gtk_image_new_from_icon_name ("object-select-symbolic");
      adw_action_row_add_suffix (ADW_ACTION_ROW (row), check);
    }

    break;

  default:
    adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                   gweather_location_get_name (location));
    break;
  }

  return row;
}

static void
row_activated_cb (GtkListBox    *listbox,
                  GtkListBoxRow *row,
                  gpointer       user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  GWeatherLocation *location;

  (void) listbox;

  location = g_object_get_data (G_OBJECT (row), "location");
  if (!location)
    return;

  switch (gweather_location_get_level (location)) {
  case GWEATHER_LOCATION_COUNTRY:
    show_cities_for_country (page, location);
    break;

  case GWEATHER_LOCATION_CITY:
    set_location (page, location);

    if (page->filter)
      gtk_filter_changed (page->filter, GTK_FILTER_CHANGE_DIFFERENT);

    break;

  default:
    break;
  }
}

static void
stop_geolocation (GisTimezonePage *page)
{
  if (page->geoclue_cancellable)
    {
      g_cancellable_cancel (page->geoclue_cancellable);
      g_clear_object (&page->geoclue_cancellable);
    }

  if (page->geoclue_client)
    {
      gclue_client_call_stop (page->geoclue_client, NULL, NULL, NULL);
      page->geoclue_client = NULL;
    }
  g_clear_object (&page->geoclue_simple);
}

static void
start_geolocation (GisTimezonePage *page)
{
  if (page->geoclue_cancellable == NULL)
    {
      page->geoclue_cancellable = g_cancellable_new ();
      get_location_from_geoclue_async (page);
    }
}

static void
on_location_settings_changed (GSettings  *settings,
                              const char *key,
                              gpointer    user_data)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (user_data);
  gboolean enabled = g_settings_get_boolean (settings, LOCATION_ENABLED_KEY);

  if (enabled)
    start_geolocation (page);
  else
    stop_geolocation (page);
}

static void
gis_timezone_page_root (GtkWidget *widget)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (widget);

  GTK_WIDGET_CLASS (gis_timezone_page_parent_class)->root (widget);

  if (g_settings_get_boolean (page->location_settings, LOCATION_ENABLED_KEY))
    start_geolocation (page);
}

static void
gis_timezone_page_constructed (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);
  g_autoptr(GError) error = NULL;
  GSettings *clock_settings;

  G_OBJECT_CLASS (gis_timezone_page_parent_class)->constructed (object);

  page->dtm_cancellable = g_cancellable_new ();

  page->dtm = timedate1_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                G_DBUS_PROXY_FLAGS_NONE,
                                                "org.freedesktop.timedate1",
                                                "/org/freedesktop/timedate1",
                                                page->dtm_cancellable,
                                                &error);
  if (page->dtm == NULL) {
    g_error ("Failed to create proxy for timedated: %s", error->message);
    exit (1);
  }

  clock_settings = g_settings_new (CLOCK_SCHEMA);
  page->clock_format = g_settings_get_enum (clock_settings, CLOCK_FORMAT_KEY);
  g_object_unref (clock_settings);

  page->world = gweather_location_get_world ();

  page->countries = g_list_store_new (GWEATHER_TYPE_LOCATION);
  page->cities = g_list_store_new (GWEATHER_TYPE_LOCATION);

  fill_countries_model (page->countries, page->world);
  fill_cities_model (page->cities, page->world);

  page->filter =
    GTK_FILTER (gtk_custom_filter_new (location_filter_match_cb,
                                       page,
                                       NULL));

  page->filter_model =
    gtk_filter_list_model_new (G_LIST_MODEL (g_object_ref (page->countries)),
                               g_object_ref (page->filter));

  page->slice_model =
    gtk_slice_list_model_new (G_LIST_MODEL (g_object_ref (page->filter_model)),
                              0,
                              G_MAXUINT);
  
  gtk_list_box_bind_model (GTK_LIST_BOX (page->listbox),
                           G_LIST_MODEL (page->slice_model),
                           create_location_row_widget,
                           page,
                           NULL);

  page->location_settings = g_settings_new (LOCATION_SCHEMA);
  g_signal_connect_object (page->location_settings,
                           "changed::" LOCATION_ENABLED_KEY,
                           G_CALLBACK (on_location_settings_changed),
                           page,
                           0);

  set_location (page, NULL);

  gtk_widget_set_visible (GTK_WIDGET (page), TRUE);
}

static void
gis_timezone_page_dispose (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);

  stop_geolocation (page);

  if (page->dtm_cancellable != NULL)
    {
      g_cancellable_cancel (page->dtm_cancellable);
      g_clear_object (&page->dtm_cancellable);
    }

  g_clear_object (&page->current_location);
  g_clear_object (&page->countries);
  g_clear_object (&page->cities);
  g_clear_object (&page->selected_country);
  g_clear_object (&page->slice_model);
  g_clear_object (&page->filter_model);
  g_clear_object (&page->filter);
  g_clear_object (&page->world);
  g_clear_object (&page->dtm);
  g_clear_object (&page->location_settings);
  g_clear_pointer (&page->casefolded_search_text, g_free);

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

static gboolean
gis_timezone_page_handle_previous (GisPage *gis_page)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (gis_page);

  if (page->showing_cities && page->selected_country != NULL) {
    show_countries (page);
    return TRUE;
  }

  return FALSE;
}

void
gis_timezone_page_shown (GisPage *gis_page)
{
        GisTimezonePage *page = GIS_TIMEZONE_PAGE (gis_page);
        gtk_widget_grab_focus (GTK_WIDGET (page->search_entry));
}

static void
gis_timezone_page_class_init (GisTimezonePageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/initial-setup/gis-timezone-page.ui");

  gtk_widget_class_bind_template_child (widget_class, GisTimezonePage, search_entry);
  gtk_widget_class_bind_template_child (widget_class, GisTimezonePage, listbox);

  gtk_widget_class_bind_template_callback (widget_class, entry_text_changed);
  gtk_widget_class_bind_template_callback (widget_class, search_entry_activated);
  gtk_widget_class_bind_template_callback (widget_class, row_activated_cb);

  page_class->page_id = PAGE_ID;
  page_class->locale_changed = gis_timezone_page_locale_changed;
  page_class->apply = gis_timezone_page_apply;
  page_class->shown = gis_timezone_page_shown;
  page_class->handle_previous = gis_timezone_page_handle_previous;
  object_class->constructed = gis_timezone_page_constructed;
  object_class->dispose = gis_timezone_page_dispose;
  widget_class->root = gis_timezone_page_root;
}

static void
gis_timezone_page_init (GisTimezonePage *page)
{
  g_type_ensure (GIS_TYPE_PAGE_HEADER);

  gtk_widget_init_template (GTK_WIDGET (page));
}

GisPage *
gis_prepare_timezone_page (GisDriver *driver)
{
  return g_object_new (GIS_TYPE_TIMEZONE_PAGE,
                       "driver", driver,
                       NULL);
}
