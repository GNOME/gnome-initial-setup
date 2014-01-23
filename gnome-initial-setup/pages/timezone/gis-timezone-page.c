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

#define GWEATHER_I_KNOW_THIS_IS_UNSTABLE
#include <libgweather/gweather.h>

#include "timedated.h"
#include "cc-datetime-resources.h"
#include "timezone-resources.h"

#include "cc-timezone-map.h"
#include "gis-bubble-widget.h"

#define DEFAULT_TZ "Europe/London"

struct _GisTimezonePagePrivate
{
  GtkWidget *map;
  GtkWidget *search_entry;
  GtkWidget *search_overlay;

  GWeatherLocation *current_location;
  Timedate1 *dtm;
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

  cc_timezone_map_set_location (CC_TIMEZONE_MAP (priv->map), location);

  gtk_widget_set_visible (priv->search_overlay, (location == NULL));
  gis_page_set_complete (GIS_PAGE (page), (location != NULL));

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
gis_timezone_page_constructed (GObject *object)
{
  GisTimezonePage *page = GIS_TIMEZONE_PAGE (object);
  GisTimezonePagePrivate *priv = gis_timezone_page_get_instance_private (page);
  GError *error;

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

  set_location (page, NULL);

  g_signal_connect (priv->search_entry, "notify::location",
                    G_CALLBACK (entry_location_changed), page);
  g_signal_connect (priv->search_entry, "map",
                    G_CALLBACK (entry_mapped), page);

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

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (klass), "/org/gnome/initial-setup/gis-timezone-page.ui");

  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisTimezonePage, map);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisTimezonePage, search_entry);
  gtk_widget_class_bind_template_child_private (GTK_WIDGET_CLASS (klass), GisTimezonePage, search_overlay);

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
